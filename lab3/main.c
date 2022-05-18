#include <linux/etherdevice.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/ip.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/udp.h>
#include <linux/version.h>
#include <net/arp.h>

#define CODE_WORD "WORD"
#define DEFAULT "lo"

static char* link = DEFAULT;
module_param(link, charp, 0);

static char* ifname = "vni%d";
static unsigned char data[1500];

static struct net_device_stats stats;
static struct net_device *child = NULL;
static struct proc_dir_entry* entry;

struct priv
{
    struct net_device *parent;
};

static int check_frame(struct sk_buff *skb, unsigned char data_shift) {
	unsigned char *user_data_ptr = NULL;
	struct iphdr *ip = (struct iphdr*)skb_network_header(skb);
	struct udphdr *udp = NULL;
	int data_len = 0;

	if (IPPROTO_UDP == ip->protocol) {
		udp = (struct udphdr*)((unsigned char*)ip + (ip->ihl * 4));
		data_len = ntohs(udp->len) - sizeof(struct udphdr);
		user_data_ptr = (unsigned char *)(skb->data + sizeof(struct iphdr)  + sizeof(struct udphdr)) + data_shift;
		memcpy(data, user_data_ptr, data_len);
		data[data_len] = '\0';
		if (strstr(data, CODE_WORD)) {
			pr_info("Captured UDP datagram, saddr: %d.%d.%d.%d\n",
		                ntohl(ip->saddr) >> 24, (ntohl(ip->saddr) >> 16) & 0x00FF,
                	        (ntohl(ip->saddr) >> 8) & 0x0000FF, (ntohl(ip->saddr)) & 0x000000FF);
			pr_info("daddr: %d.%d.%d.%d\n",
			        ntohl(ip->daddr) >> 24, (ntohl(ip->daddr) >> 16) & 0x00FF,
			        (ntohl(ip->daddr) >> 8) & 0x0000FF, (ntohl(ip->daddr)) & 0x000000FF);

			pr_info("Data length: %d. Data:\n", data_len);
			pr_info("%s\n", data);
			return 1;
		}
		pr_info("data_length: %d, data: %s\n", data_len, data);
		return 0;
	}
}

static rx_handler_result_t handle_frame(struct sk_buff **pskb)
{
	struct sk_buff * skb = * pskb;

	stats.rx_packets++;
	stats.rx_bytes += skb->len;

	if (!check_frame(skb, 0)) {
		stats.tx_packets++;
		stats.tx_bytes += skb->len;
		return RX_HANDLER_PASS;
	}

	return RX_HANDLER_CONSUMED;
}

static int open(struct net_device *dev)
{
	netif_start_queue(dev);
	pr_info("%s: device opened", dev->name);
	return 0;
}

static int stop(struct net_device *dev)
{
	netif_stop_queue(dev);
	pr_info("%s: device closed", dev->name);
	return 0;
}

static netdev_tx_t start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct priv *priv = netdev_priv(dev);

    if (check_frame(skb, 14)) {
        stats.tx_packets++;
        stats.tx_bytes += skb->len;
    }

    if (priv->parent) {
        skb->dev = priv->parent;
        skb->priority = 1;
        dev_queue_xmit(skb);
        return 0;
    }
    return NETDEV_TX_OK;
}

static struct net_device_stats *get_stats(struct net_device *dev)
{
    return &stats;
}

static struct net_device_ops net_device_ops = {
    .ndo_open = open,
    .ndo_stop = stop,
    .ndo_get_stats = get_stats,
    .ndo_start_xmit = start_xmit};

static void setup(struct net_device *dev)
{
    ether_setup(dev);
    memset(netdev_priv(dev), 0, sizeof(struct priv));
    dev->netdev_ops = &net_device_ops;
}

static ssize_t proc_write(struct file *file, const char __user * ubuf, size_t count, loff_t* ppos) {
	pr_info("%s: Proc file write attempt", THIS_MODULE->name);
	return -1;
}

static ssize_t proc_read(struct file *file, char __user * ubuf, size_t count, loff_t* ppos) {
	char *stat;
	size_t len;

	stat = kmalloc_array(count, sizeof(char), GFP_KERNEL);

	len = sprintf(stat, "Rx packets: %ld, Rx bytes: %ld\nTx packets: %ld, Tx bytes: %ld\n", stats.rx_packets, stats.rx_bytes, stats.tx_packets, stats.tx_bytes);
	if (*ppos > 0 || count < len)
		return 0;
	
	if (copy_to_user(ubuf, stat, len) != 0)
		return -EFAULT;

	*ppos = len;
	kfree(stat);
	return len;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = proc_read,
	.write = proc_write,
};

int __init lab3_init(void)
{
	int err = 0;
	struct priv *priv;
	child = alloc_netdev(sizeof(struct priv), ifname, NET_NAME_UNKNOWN, setup);
	if (!child) {
		pr_err("%s: Allocation error", THIS_MODULE->name);
		return -ENOMEM;
	}
	priv = netdev_priv(child);
	priv->parent = __dev_get_by_name(&init_net, link);
	if (!priv->parent) {
		pr_err("%s: No such net: %s", THIS_MODULE->name, link);
		free_netdev(child);
		return -ENODEV;
	}
	if (priv->parent->type != ARPHRD_ETHER && priv->parent->type != ARPHRD_LOOPBACK) {
		pr_err("%s: Illegal net type", THIS_MODULE->name); 
		free_netdev(child);
        	return -EINVAL;
	}

	memcpy(child->dev_addr, priv->parent->dev_addr, ETH_ALEN);
	memcpy(child->broadcast, priv->parent->broadcast, ETH_ALEN);
	if ((err = dev_alloc_name(child, child->name))) {
		pr_err("%s: Allocate name, error %i", THIS_MODULE->name, err);
		free_netdev(child);
		return -EIO;
	}

	register_netdev(child);
	rtnl_lock();
	netdev_rx_handler_register(priv->parent, &handle_frame, NULL);
	rtnl_unlock();
	
	stats.tx_packets = 0;
	stats.tx_bytes = 0;
	stats.rx_packets = 0;
	stats.rx_bytes = 0;

	entry = proc_create("var4", 0444, NULL, &fops);

	pr_info("%s: Module loaded", THIS_MODULE->name);
	pr_info("%s: Created link %s", THIS_MODULE->name, child->name);
	pr_info("%s: Registered rx handler for %s", THIS_MODULE->name, priv->parent->name);
	return 0;
}

void __exit lab3_exit(void) {
	struct priv *priv = netdev_priv(child);
	if (priv->parent) {
		rtnl_lock();
		netdev_rx_handler_unregister(priv->parent);
		rtnl_unlock();
		pr_info("%s: Unregistered rx handler for %s", THIS_MODULE->name, priv->parent->name);
	}
	unregister_netdev(child);
	free_netdev(child);

	proc_remove(entry);
	pr_info("%s: Module unloaded", THIS_MODULE->name); 
} 

module_init(lab3_init);
module_exit(lab3_exit);

MODULE_AUTHOR("P33301 Geller Leonid & Blinova Margarita");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IO systems lab 3 - network device");
