#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/mmzone.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leo Stone");
MODULE_DESCRIPTION("LKP - Project 4 Fragmentation Indicator");

/* Copied from mm/vmstat.c */
/*
 * Walk zones in a node and print using a callback.
 * If @assert_populated is true, only use callback for zones that are populated.
 */
static void walk_zones_in_node(struct seq_file *m, pg_data_t *pgdat,
		bool assert_populated, bool nolock,
		void (*print)(struct seq_file *m, pg_data_t *, struct zone *))
{
	struct zone *zone;
	struct zone *node_zones = pgdat->node_zones;
	unsigned long flags;

	for (zone = node_zones; zone - node_zones < MAX_NR_ZONES; ++zone) {
		if (assert_populated && !populated_zone(zone))
			continue;

		if (!nolock)
			spin_lock_irqsave(&zone->lock, flags);
		print(m, pgdat, zone);
		if (!nolock)
			spin_unlock_irqrestore(&zone->lock, flags);
	}
}

static void frag_show_print(struct seq_file *m, pg_data_t *pgdat, struct zone *zone)
{
	int order;

	seq_printf(m, "Node %d, zone %8s ", pgdat->node_id, zone->name);
	for(order = 0; order < MAX_ORDER; ++order)
		seq_printf(m, "%6lu ", zone->free_area[order].nr_free);
	seq_putc(m, '\n');
}

static int frag_proc_show(struct seq_file *m, void *v)
{
	/* Note: This will only get complete info for UMA machines. */
	pg_data_t *pgdat = NODE_DATA(0);
	walk_zones_in_node(m, pgdat, true, false, frag_show_print);
	return 0;
}

static int frag_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, frag_proc_show, NULL);
}

static const struct proc_ops frag_proc_fops = {
	.proc_open = frag_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init frag_init(void)
{
	proc_create("frag", 0, NULL, &frag_proc_fops);
	return 0;
}

static void __exit frag_exit(void)
{
	remove_proc_entry("frag", NULL);
	return;
}

module_init(frag_init);
module_exit(frag_exit);
