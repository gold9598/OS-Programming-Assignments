/*
 * Weighted Round-Robin Scheduling Class (mapped to SCHED_WRR policy
 */

#define BASE_WRR_TIMESLICE	(10 * HZ / 1000)

#define DEFAULT_WRR_WEIGHT	10

static const struct sched_class wrr_sched_class;

#ifdef CONFIG_SMP
static struct hrtimer wrr_load_timer;
static unsigned int sched_wrr_load_period = 500000;
#endif

static inline int on_wrr_rq(struct sched_wrr_entity *wrr_se)
{
	return !list_empty(&wrr_se->run_list);
}

static unsigned int get_wrr_timeslice(int weight)
{
	return BASE_WRR_TIMESLICE * weight;
}

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *swe)
{
	return container_of(swe, struct task_struct, wrr);
}

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq)
{
	return container_of(wrr_rq, struct rq, wrr);
}

static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se)
{
	struct task_struct *p = wrr_task_of(wrr_se);
	struct rq *rq = task_rq(p);

	return &rq->wrr;
}

static inline void inc_wrr_tasks(struct wrr_rq *wrr_rq)
{
	wrr_rq->wrr_nr_running++;
}

static inline void dec_wrr_tasks(struct wrr_rq *wrr_rq)
{
	wrr_rq->wrr_nr_running--;
}

#ifdef CONFIG_SMP

static inline void add_wrr_rq_weight(struct wrr_rq *wrr_rq, int weight)
{
	wrr_rq->weight_sum += weight;
}

static inline void sub_wrr_rq_weight(struct wrr_rq *wrr_rq, int weight)
{
	wrr_rq->weight_sum -= weight;
	if (wrr_rq->weight_sum < 0)
		wrr_rq->weight_sum = 0;
}

static void __enqueue_plist_task(struct wrr_rq *wrr_rq,
				struct task_struct *p)
{
	/*
	 * We use the same list head rt uses.
	 * We assume a task cannot be on an rt list
	 * and a wrr list simultaneously
	 */

	plist_del(&p->pushable_tasks, &wrr_rq->movable_tasks);
	plist_node_init(&p->pushable_tasks, p->wrr_weight);
	plist_add(&p->pushable_tasks, &wrr_rq->movable_tasks);
}

static void __dequeue_plist_task(struct wrr_rq *wrr_rq,
				struct task_struct *p)
{
	plist_del(&p->pushable_tasks, &wrr_rq->movable_tasks);
}

/*
 * Find the cpu with the lowest weight wrr_rq
 */
static int find_lowest_wrr_rq(void)
{
	int cpu;
	int min_weight;
	int min_cpu;
	struct rq *rq;
	struct wrr_rq *wrr_rq;

	cpu = smp_processor_id();
	min_weight = INT_MAX;
	min_cpu = cpu;

	for_each_cpu(cpu, cpu_online_mask) {
		rq = cpu_rq(cpu);
		wrr_rq = &rq->wrr;

		raw_spin_lock(&rq->lock);

		if (rq->wrr.weight_sum < min_weight) {
			min_weight = rq->wrr.weight_sum;
			min_cpu = cpu;
		}

		raw_spin_unlock(&rq->lock);
	}

	return min_cpu;
}

/*
 * Find the cpu with the highest weight wrr_rq
 */
static int find_highest_wrr_rq(void)
{
	int cpu;
	int max_weight;
	int max_cpu;
	struct rq *rq;
	struct wrr_rq *wrr_rq;

	cpu = smp_processor_id();
	max_weight = 0;
	max_cpu = cpu;

	for_each_cpu(cpu, cpu_online_mask) {
		rq = cpu_rq(cpu);
		wrr_rq = &rq->wrr;

		raw_spin_lock(&rq->lock);

		if (rq->wrr.weight_sum > max_weight) {
			max_weight = rq->wrr.weight_sum;
			max_cpu = cpu;
		}

		raw_spin_unlock(&rq->lock);

	}

	return max_cpu;
}

/*
 * Find an eligible job from the highest weight wrr_rq
 * and move to lowest weight wrr_rq.
 */
static void find_and_move_job(int lowest_cpu, int highest_cpu)
{

	struct wrr_rq *lowest_wrr_rq;
	struct wrr_rq *highest_wrr_rq;
	struct rq *lowest_rq;
	struct rq *highest_rq;
	struct task_struct *p;
	int found = 0;
	int low_new_weight;
	int high_new_weight;

	if (lowest_cpu == highest_cpu)
		return;

	lowest_rq = cpu_rq(lowest_cpu);
	highest_rq = cpu_rq(highest_cpu);

	double_rq_lock(lowest_rq, highest_rq);

	lowest_wrr_rq = &lowest_rq->wrr;
	highest_wrr_rq = &highest_rq->wrr;

	plist_for_each_entry(p, &highest_wrr_rq->movable_tasks,
					pushable_tasks) {

		if (task_running(highest_rq, p))
			continue;

		/* Is the task allowed on lowest_cpu? if not, dont pick */
		if (!cpumask_test_cpu(lowest_cpu, &p->cpus_allowed))
			continue;

		/*
		 * Check that the move wont cause the
		 * weight imbalance to reverse.
		 */
		low_new_weight = lowest_wrr_rq->weight_sum + p->wrr_weight;
		high_new_weight = highest_wrr_rq->weight_sum - p->wrr_weight;
		if (low_new_weight >= high_new_weight)
			continue;

		/*
		 * At this point process IS allowed on lowest_cpu,
		 * so break from here and move it
		 */
		found = 1;
		break;

	}


	if (p != NULL && found) {
		BUG_ON(task_running(highest_rq, p));
		deactivate_task(highest_rq, p, 0);
		set_task_cpu(p, lowest_cpu);
		activate_task(lowest_rq, p, 0);
	}

	double_rq_unlock(lowest_rq, highest_rq);
}

static int do_sched_wrr_load_timer(void)
{
	int lowest_cpu, highest_cpu;

	lowest_cpu = find_lowest_wrr_rq();

	highest_cpu = find_highest_wrr_rq();

	if (lowest_cpu == highest_cpu)
		return 0;

	rcu_read_lock();

	find_and_move_job(lowest_cpu, highest_cpu);

	rcu_read_unlock();

	return 0;
}

/* Periodic load balancing timer for wrr sched class */
static inline u64 global_wrr_load_period(void)
{
	return (u64)sched_wrr_load_period * NSEC_PER_USEC;
}

static enum hrtimer_restart sched_wrr_load_timer(struct hrtimer *timer)
{
	ktime_t now;

	ktime_t load_period = ns_to_ktime(global_wrr_load_period());

	now = hrtimer_cb_get_time(timer);
	hrtimer_forward(timer, now, load_period);

	do_sched_wrr_load_timer();

	return HRTIMER_RESTART;
}

static void init_wrr_load_balance(void)
{
	hrtimer_init(&wrr_load_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	wrr_load_timer.function = sched_wrr_load_timer;
}

static void start_wrr_load_balance(void)
{
	struct hrtimer *timer = &wrr_load_timer;

	ktime_t now;

	ktime_t load_period = ns_to_ktime(global_wrr_load_period());

	if (hrtimer_active(timer))
		return;

	for (;;) {

		if (hrtimer_active(timer))
			break;

		now = hrtimer_cb_get_time(timer);
		hrtimer_forward(timer, now, load_period);

		hrtimer_start(timer, load_period,
				HRTIMER_MODE_ABS_PINNED);
	}
}

#else /* !CONFIG_SMP */

static inline void add_wrr_rq_weight(struct wrr_rq *wrr_rq, int weight)
{
}

static inline void sub_wrr_rq_weight(struct wrr_rq *wrr_rq, int weight)
{
}

static inline
void __enqueue_plist_task(struct wrr_rq *wrr_rq, struct task_struct *p)
{
}

static inline
void __dequeue_plist_task(struct wrr_rq *wrr_rq, struct task_struct *p)
{
}

#endif

/* rq lock must be held */
static void __wrr_set_weight(struct rq *rq, struct task_struct *p, int weight)
{
	/* Reset the wrr_rq weight while setting new weight for p */
	sub_wrr_rq_weight(&rq->wrr, p->wrr_weight);
	p->wrr_weight = weight;
	add_wrr_rq_weight(&rq->wrr, p->wrr_weight);

	if (!task_running(rq, p) && on_wrr_rq(&p->wrr))
		__enqueue_plist_task(&rq->wrr, p);
}

static void update_curr_wrr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	u64 delta_exec;

	if (curr->sched_class != &wrr_sched_class)
		return;

	delta_exec = rq->clock_task - curr->se.exec_start;
	if (unlikely((s64)delta_exec < 0))
		delta_exec = 0;

	schedstat_set(curr->se.statistics.exec_max,
		max(curr->se.statistics.exec_max, delta_exec));


	/* Update the entity's runtime */
	curr->se.sum_exec_runtime += delta_exec;

	account_group_exec_runtime(curr, delta_exec);

	/* Reset the start time */
	curr->se.exec_start = rq->clock_task;

	cpuacct_charge(curr, delta_exec);

}

/*
 * Moves p to the back of the queue
 */
static void requeue_task_wrr(struct rq *rq, struct task_struct *p)
{
	struct list_head *wrr_task_list;
	struct sched_wrr_entity *swe;

	wrr_task_list = &(rq->wrr.wrr_list);
	swe = &p->wrr;

	/* Recalc timeslice here in case of weight change */
	swe->time_slice	= get_wrr_timeslice(p->wrr_weight);

	/* Weight may have changed so place it in plist again */
	__enqueue_plist_task(&rq->wrr, p);

	list_move_tail(&swe->run_list, wrr_task_list);
}

static void __enqueue_wrr_entity(struct wrr_rq *wrr_rq,
				struct sched_wrr_entity *wrr_se,
				int weight)
{

	struct list_head *wrr_task_list = &wrr_rq->wrr_list;

	list_add_tail(&wrr_se->run_list, wrr_task_list);

	inc_wrr_tasks(wrr_rq);

	/* Recalc timeslice here in case of task weight change */
	wrr_se->time_slice = get_wrr_timeslice(weight);

	/* Add the weight of the task to the rq weight sum */
	add_wrr_rq_weight(wrr_rq, weight);
}

/*
 * Add a task to the wrr list
 */
static
void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct wrr_rq *wrr_rq;
	struct sched_wrr_entity *wrr_se;
	int weight;

	wrr_rq = &rq->wrr;
	wrr_se = &p->wrr;
	weight = p->wrr_weight;

	__enqueue_wrr_entity(wrr_rq, wrr_se, weight);

	if (!task_current(rq, p))
		__enqueue_plist_task(wrr_rq, p);
}

static void __dequeue_wrr_entity(struct wrr_rq *wrr_rq,
				struct sched_wrr_entity *wrr_se,
				int weight)
{
	if (on_wrr_rq(wrr_se))
		list_del_init(&wrr_se->run_list);

	dec_wrr_tasks(wrr_rq);

	/* Update weight of the wrr_rq */
	sub_wrr_rq_weight(wrr_rq, weight);
}

/*
 * Remove a task from the wrr list
 */
static
void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct wrr_rq *wrr_rq;
	struct sched_wrr_entity *wrr_se;
	int weight;

	wrr_rq = &rq->wrr;
	wrr_se = &p->wrr;
	weight = p->wrr_weight;

	update_curr_wrr(rq);

	if (on_wrr_rq(wrr_se)) {
		__dequeue_wrr_entity(wrr_rq, wrr_se, weight);
		__dequeue_plist_task(wrr_rq, p);
	}
}

static void yield_task_wrr(struct rq *rq)
{
	requeue_task_wrr(rq, rq->curr);
}

static
void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
}

static struct sched_wrr_entity *get_first_entity(struct wrr_rq *wrr_rq)
{
	struct sched_wrr_entity *swe;
	struct list_head *wrr_task_list;

	wrr_task_list = &wrr_rq->wrr_list;

	swe = list_entry(wrr_task_list->next,
		struct sched_wrr_entity, run_list);

	return swe;
}

static struct task_struct *__pick_next_entity_wrr(struct rq *rq)
{
	struct sched_wrr_entity *swe;
	struct task_struct *p;
	struct wrr_rq *wrr_rq;

	wrr_rq = &rq->wrr;

	if (!wrr_rq->wrr_nr_running)
		return NULL;

	swe = get_first_entity(wrr_rq);

	p = wrr_task_of(swe);
	p->se.exec_start = rq->clock_task;

	return p;
}

/*
 * Returns the next task to be scheduled from the wrr_rq
 */
static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
	struct task_struct *p = __pick_next_entity_wrr(rq);

	/* Currently running task is not eligible to be moved */
	if (p)
		__dequeue_plist_task(&(rq->wrr), p);

	return p;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *prev)
{
	update_curr_wrr(rq);
	prev->se.exec_start = 0;

	/* If task is still active, it is now eligible to be moved again */
	if (on_wrr_rq(&prev->wrr))
		__enqueue_plist_task(&rq->wrr, prev);
}

#ifdef CONFIG_SMP
/*
 * NEED PREEMPTION DISABLED -- FORK/WAKEUP ALREADY HOLD pi_lock
 *
 * Select a cpu on which to place task p
 */
static
int select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags)
{
	int cpu;
	int target;

	rcu_read_lock();

	cpu = task_cpu(p);
	target = -1;

	if (p->policy == SCHED_WRR)
		target = find_lowest_wrr_rq();

	if (target != -1)
		cpu = target;

	rcu_read_unlock();

	return cpu;
}
#endif

static void set_curr_task_wrr(struct rq *rq)
{
	struct task_struct *p = rq->curr;

	p->se.exec_start = rq->clock_task;

	__dequeue_plist_task(&(rq->wrr), p);
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
	update_curr_wrr(rq);

	if (--p->wrr.time_slice > 0)
		return;

	/* No time left, refill */
	p->wrr.time_slice = get_wrr_timeslice(p->wrr_weight);

	/* If we are not the only ones on the list, requeue and resched	 */
	if (p->wrr.run_list.prev != p->wrr.run_list.next) {
		requeue_task_wrr(rq, p);
		set_tsk_need_resched(p);
	}
}

/*
 * Return the interval alloted to task
 */
static
unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
	return get_wrr_timeslice(task->wrr_weight);
}

static
void prio_changed_wrr(struct rq *rq, struct task_struct *p, int prio)
{
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
}

static void task_fork_wrr(struct task_struct *p)
{
	struct sched_wrr_entity *swe;
	struct rq *rq;

	rq = this_rq();

	if (p->wrr_weight < 1 || p->wrr_weight > 20)
		p->wrr_weight = DEFAULT_WRR_WEIGHT;

	swe			= &p->wrr;
	swe->time_slice		= get_wrr_timeslice(p->wrr_weight);

#ifdef CONFIG_SMP
	start_wrr_load_balance();
#endif
}

static const struct sched_class wrr_sched_class = {

	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
#endif

	.set_curr_task		= set_curr_task_wrr,
	.task_tick		= task_tick_wrr,

	.get_rr_interval	= get_rr_interval_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_to		= switched_to_wrr,
	.task_fork		= task_fork_wrr,
};
