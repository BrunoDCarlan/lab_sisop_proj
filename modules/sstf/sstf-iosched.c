/*
 * SSTF IO Scheduler
 *
 * For Kernel 4.13.9
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

/* SSTF data structure. */
struct sstf_data {
	struct list_head queue;
	sector_t head_pos;
};

static void sstf_reorder(struct sstf_data *nd)
{
	LIST_HEAD(pending);
	struct request *candidate, *best;
	sector_t simulated_head = nd->head_pos;
	sector_t distance, best_distance;

	/* Retira temporariamente todos os pedidos da fila. */
	list_splice_init(&nd->queue, &pending);

	while (!list_empty(&pending)) {
		best = NULL;
		best_distance = 0;

		/* Encontra o pedido mais próximo da posição simulada. */
		list_for_each_entry(candidate, &pending, queuelist) {
			sector_t sector = blk_rq_pos(candidate);

			distance = sector >= simulated_head
				? sector - simulated_head
				: simulated_head - sector;

			if (!best || distance < best_distance) {
				best = candidate;
				best_distance = distance;
			}
		}

		/* Coloca o escolhido no fim da sequência de atendimento. */
		list_move_tail(&best->queuelist, &nd->queue);
		simulated_head = blk_rq_pos(best);
	}
}

static void sstf_merged_requests(struct request_queue *q,
				 struct request *rq,
				 struct request *next)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	list_del_init(&next->queuelist);
	sstf_reorder(nd);
}

/* Esta função despacha o próximo bloco a ser lido. */
static int sstf_dispatch(struct request_queue *q, int force)
{
	struct sstf_data *nd = q->elevator->elevator_data;
	struct request *rq;
	sector_t sector;
	char direction;

	rq = list_first_entry_or_null(&nd->queue,
				      struct request, queuelist);
	if (!rq)
		return 0;

	sector = blk_rq_pos(rq);
	direction = rq_data_dir(rq) ? 'W' : 'R';

	list_del_init(&rq->queuelist);
	nd->head_pos = sector;
	elv_dispatch_add_tail(q, rq);

	printk(KERN_INFO "[SSTF] dsp %c %llu\n",
	       direction, (unsigned long long)sector);
	return 1;
}

static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;
	char direction = rq_data_dir(rq) ? 'W' : 'R';

	list_add_tail(&rq->queuelist, &nd->queue);
	sstf_reorder(nd);

	printk(KERN_INFO "[SSTF] add %c %llu\n",
	       direction, (unsigned long long)blk_rq_pos(rq));
}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e){
	struct sstf_data *nd;
	struct elevator_queue *eq;

	/* Implementação da inicialização da fila (queue).
	 *
	 * Use como exemplo a inicialização da fila no driver noop-iosched.c
	 *
	 */

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = nd;

	INIT_LIST_HEAD(&nd->queue);
	nd->head_pos = 0;

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

	return 0;
}

static void sstf_exit_queue(struct elevator_queue *e)
{
	struct sstf_data *nd = e->elevator_data;

	/* Implementação da finalização da fila (queue).
	 *
	 * Use como exemplo o driver noop-iosched.c
	 *
	 */
	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

/* Infrastrutura dos drivers de IO Scheduling. */
static struct elevator_type elevator_sstf = {
	.ops.sq = {
		.elevator_merge_req_fn		= sstf_merged_requests,
		.elevator_dispatch_fn		= sstf_dispatch,
		.elevator_add_req_fn		= sstf_add_request,
		.elevator_init_fn		= sstf_init_queue,
		.elevator_exit_fn		= sstf_exit_queue,
	},
	.elevator_name = "sstf",
	.elevator_owner = THIS_MODULE,
};

/* Inicialização do driver. */
static int __init sstf_init(void)
{
	return elv_register(&elevator_sstf);
}

/* Finalização do driver. */
static void __exit sstf_exit(void)
{
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);

MODULE_AUTHOR("Miguel Xavier");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO scheduler");
