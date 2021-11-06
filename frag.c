#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/mmzone.h>
#include <linux/spinlock.h>
#include <linux/timekeeping.h>
#include <linux/timer.h>
#include <linux/list.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leo Stone");
MODULE_DESCRIPTION("LKP - Project 4 Fragmentation Indicator");

/* Globals used by timer callback */
static int recording = 0; 	/* Nonzero if currently recording */
static int rate = 1;		/* Seconds between timer callbacks */

struct frag_sample {
	struct list_head list;
	ktime_t timestamp;
	long unsigned int nr_free[MAX_ORDER];
};

/* Sample list */
static LIST_HEAD(sample_list);

/* Adapted from mm/vmstat.c */
/*
 * Walk zones in a node and count using a callback.
 * If @assert_populated is true, only use callback for zones that are populated.
 */
static void count_zones_in_node(pg_data_t *pgdat, bool assert_populated, bool nolock,
                void (*count)(pg_data_t *, struct zone *, struct frag_sample *), struct frag_sample *sample)
{
        struct zone *zone;
        struct zone *node_zones = pgdat->node_zones;
        unsigned long flags;

        for (zone = node_zones; zone - node_zones < MAX_NR_ZONES; ++zone) {
                if (assert_populated && !populated_zone(zone))
                        continue;

                if (!nolock)
                        spin_lock_irqsave(&zone->lock, flags);
                count(pgdat, zone, sample);
                if (!nolock)
                        spin_unlock_irqrestore(&zone->lock, flags);
        }
}

static void count_nr_free(pg_data_t *pgdat, struct zone *zone, struct frag_sample *new_sample)
{
	int order;

	for(order = 0; order < MAX_ORDER; ++order)
		new_sample->nr_free[order] += zone->free_area[order].nr_free;
}


static void add_new_sample(ktime_t time)
{
	pg_data_t *pgdat;
	struct frag_sample *new_sample = kmalloc(sizeof(struct frag_sample), GFP_KERNEL);
	new_sample->timestamp = time;
	memset(&new_sample->nr_free[0], 0, sizeof(long unsigned int) * MAX_ORDER);

	/* Note: This will only get complete info for UMA machines. */
	pgdat = NODE_DATA(0);
	count_zones_in_node(pgdat, true, false, count_nr_free, new_sample);

	list_add_tail(&new_sample->list, &sample_list);
}

static void destroy_list_and_free(void)
{
	struct frag_sample *current_sample, *next;
	list_for_each_entry_safe(current_sample, next, &sample_list, list) {
		list_del(&current_sample->list);
		kfree(current_sample);
	}
	INIT_LIST_HEAD(&sample_list);
}

static struct timer_list sample_timer;

/* Timer callback */
static void sample_timer_callback(struct timer_list*);

static void arm_timer(void)
{
	mod_timer(&sample_timer, jiffies + msecs_to_jiffies(rate * 1000));
}

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

/* /proc entry to start/stop recording */
static int record_proc_show(struct seq_file *m, void *v)
{
	recording = !recording;
	if(recording) {
		destroy_list_and_free();
		seq_printf(m, "Recording started.\n");
		timer_setup(&sample_timer, sample_timer_callback, 0);
		arm_timer();
	} else {
		seq_printf(m, "Recording stopped.\n");
		del_timer(&sample_timer);
	}
	return 0;
}

static int record_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, record_proc_show, NULL);
}

static const struct proc_ops record_proc_fops = {
	.proc_open = record_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/* /proc entry to output last recording in CSV format */
static int recording_proc_show(struct seq_file *m, void *v)
{
	struct frag_sample *current_sample;
	int i;
	seq_printf(m, "time,");
	for(i = 0; i < MAX_ORDER; i++) {
		seq_printf(m, "%d", i);
		if(i < MAX_ORDER - 1)
			seq_printf(m, ",");
	}
	seq_putc(m, '\n');
	list_for_each_entry(current_sample, &sample_list, list) {
		seq_printf(m, "%lld,", current_sample->timestamp);
		for(i = 0; i < MAX_ORDER; i++) {
			seq_printf(m, "%lu", current_sample->nr_free[i]); 
                	if(i < MAX_ORDER - 1)
                        	seq_printf(m, ",");  		
		}
		seq_putc(m, '\n');
	}
	return 0;
}

static int recording_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, recording_proc_show, NULL);
}

static const struct proc_ops recording_proc_fops = {
	.proc_open = recording_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void sample_timer_callback(struct timer_list *timer)
{
	printk(KERN_INFO "%d seconds elapsed\n", rate);
	add_new_sample(ktime_get_real());
	if(recording) {
		arm_timer();
	}
}

static int __init frag_init(void)
{
	struct proc_dir_entry *frag_dir = proc_mkdir("frag", NULL);
	proc_create("info", 0, frag_dir, &frag_proc_fops);
	proc_create("record", 0, frag_dir, &record_proc_fops);
	proc_create("data", 0, frag_dir, &recording_proc_fops);
	return 0;
}

static void __exit frag_exit(void)
{
	remove_proc_subtree("frag", NULL);
	if(recording) {
		del_timer(&sample_timer);
	}
	destroy_list_and_free();
}

module_init(frag_init);
module_exit(frag_exit);
