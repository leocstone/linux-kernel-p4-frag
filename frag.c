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
#include <linux/string.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/compaction.h>
#include <linux/gfp.h>
#include <linux/stacktrace.h>
#include <linux/sched.h>
#include <asm/msr.h>
#include <linux/compaction.h>
#include <linux/mm.h>
#include <linux/swap.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leo Stone");
MODULE_DESCRIPTION("LKP - Project 4 Fragmentation Indicator");


/* These functions are used for the compaction routine.
 * They will have their actual kernel functions found by
 * registering (and unregistering) dummy kprobes to them */
void (*my_compact_node) (int);
void (*my_lru_add_drain_all) (void);

/* This function performs the compaction manually 
 * for all NUMA nodes. This code was taken from the
 * /proc/sys/vm/compact_nodes kernel interface */
static void compact_nodes(void){

	int nid;

	my_lru_add_drain_all();

	for_each_online_node(nid){
		my_compact_node(nid);
	}
}

/* This is the skeleton code we need to setup the dummy kprobes*/
// Since kernel v5.7, we can't use kallsyms_lookup_name
// so we instead need to query kprobes for the address of functions
static int __kprobes dummy_pre_handler(struct kprobe *p, struct pt_regs *regs){
	return 0;
}

static void __kprobes dummy_post_handler(struct kprobe*p, struct pt_regs *regs, unsigned long flags){
	return;
}

// Let's make a function to get any function symbol we want by name
static u8* get_ksymbol_by_name(const char* func_name){

	int ret;
	u8* target_func = 0;

	struct kprobe dummy_probe= {
		.symbol_name		= func_name,
		.pre_handler		= dummy_pre_handler,
		.post_handler		= dummy_post_handler,
	};

	ret = register_kprobe(&dummy_probe);
	if(ret < 0){
		pr_err("register_kprobe failed, returned %d\n", ret);
		return 0;
	}

	// Print the address for the function of interest
	pr_info("Planted kprobe at %s: %p\n", dummy_probe.symbol_name, dummy_probe.addr);

	target_func = dummy_probe.addr;

	unregister_kprobe(&dummy_probe);

	return target_func;
}

/* Globals used by timer callback */
static int recording = 0; 	/* Nonzero if currently recording */
static int rate = 3;		/* Seconds between timer callbacks */

module_param(rate, int, 0660);

struct frag_sample {
	struct list_head list;
	ktime_t timestamp;
	long unsigned int nr_free[MAX_ORDER];
	long unsigned int usable_pages[MAX_ORDER];
	long unsigned int free_pages;
};

/* Sample list */
static LIST_HEAD(sample_list);

static DEFINE_SPINLOCK(list_lock);

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
	int j;

	/* For now, only track fragmentation in the Normal zone */
	if(strcmp(zone->name, "Normal"))
		return;

	for(order = 0; order < MAX_ORDER; ++order) {
		new_sample->nr_free[order] += zone->free_area[order].nr_free;
		new_sample->free_pages += (1L << order) * new_sample->nr_free[order];
	}

	for(order = 0; order < MAX_ORDER; ++order) {
		for(j = order; j < MAX_ORDER; j++) {
			new_sample->usable_pages[order] += (1L << j) * new_sample->nr_free[j];
		}	
	}
}

static void add_new_sample(ktime_t time)
{
	pg_data_t *pgdat;
	struct frag_sample *new_sample = kmalloc(sizeof(struct frag_sample), GFP_KERNEL);
	new_sample->timestamp = time;
	memset(&new_sample->nr_free[0], 0, sizeof(long unsigned int) * MAX_ORDER);
	memset(&new_sample->usable_pages[0], 0, sizeof(long unsigned int) * MAX_ORDER);
	new_sample->free_pages = 0;

	/* Note: This will only get complete info for UMA machines. */
	pgdat = NODE_DATA(0);
	count_zones_in_node(pgdat, true, false, count_nr_free, new_sample);

	spin_lock(&list_lock);
	list_add_tail(&new_sample->list, &sample_list);
	spin_unlock(&list_lock);
}

static void destroy_list_and_free(void)
{
	struct frag_sample *current_sample, *next;
	spin_lock(&list_lock);
	list_for_each_entry_safe(current_sample, next, &sample_list, list) {
		list_del(&current_sample->list);
		kfree(current_sample);
	}
	INIT_LIST_HEAD(&sample_list);
	spin_unlock(&list_lock);
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
	int nid;
	pg_data_t *pgdat;

	for_each_online_node(nid)
		pgdat = NODE_DATA(nid);
		walk_zones_in_node(m, pgdat, true, false, frag_show_print);

	// Perform compaction and see the difference
	compact_nodes();

	for_each_online_node(nid)
		pgdat = NODE_DATA(nid);
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
		seq_printf(m, "unusable_free_space_index_%d,free_blocks_%d", i, i);
		if(i < MAX_ORDER - 1)
			seq_printf(m, ",");
	}
	seq_putc(m, '\n');
	spin_lock(&list_lock);
	list_for_each_entry(current_sample, &sample_list, list) {
		seq_printf(m, "%lld,", current_sample->timestamp);
		for(i = 0; i < MAX_ORDER; i++) {
			seq_printf(m, "%lu/", (current_sample->free_pages - current_sample->usable_pages[i]));
			seq_printf(m, "%lu,", current_sample->free_pages);
			seq_printf(m, "%lu", current_sample->nr_free[i]); 
                	if(i < MAX_ORDER - 1)
                        	seq_printf(m, ",");  		
		}
		seq_putc(m, '\n');
	}
	spin_unlock(&list_lock);
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
	add_new_sample(ktime_get_real());
	if(recording) {
		arm_timer();
	}
}

static int __init frag_init(void)
{
	struct proc_dir_entry *frag_dir;
	if(rate <= 0) {
		printk(KERN_WARNING "Invalid rate parameter. Exiting.\n");
		return -1;
	}

	/* Let's do the function lookups so we can call compact_nodes*/
	my_compact_node = (void (*) (int)) get_ksymbol_by_name("compact_node");
	if(!my_compact_node){
		pr_err("FAILED TO PERFORM SYMBOL LOOKUP \n");
		return -1;
	}

	my_lru_add_drain_all = (void (*) (void)) get_ksymbol_by_name("lru_add_drain_all");
	if(!my_lru_add_drain_all){
		pr_err("FAILED TO PERFORM SYMBOL LOOKUP \n");
		return -1;
	}

	frag_dir = proc_mkdir("frag", NULL);
	proc_create("info", 0, frag_dir, &frag_proc_fops);
	proc_create("record", 0, frag_dir, &record_proc_fops);
	proc_create("last_recording", 0, frag_dir, &recording_proc_fops);


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
