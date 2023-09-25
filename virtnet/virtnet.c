#define pr_fmt(fmt) "virtnet[%d]: " fmt, __LINE__

#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#define VNET_MAX_MTU (64 * 1024)

struct vnet_priv {
	struct sk_buff *skb;
	struct net_device *vnetdev;
	struct napi_struct napi;
	struct net_device_stats stats;
};

static int debug = 0;

/* Receive Data Logic Reference */
int virtnet_rx(struct vnet_priv *priv, int datalen)
{
	struct sk_buff *skb;

	skb = dev_alloc_skb(datalen + 2);
	if (!skb)
		return -1;
	
	/* align IP on 16B boundary */
	skb_reserve(skb, 2);
	
	/* Logic for receiving data */

	skb->dev = priv->vnetdev;
	skb->protocol = eth_type_trans(skb, skb->dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	if (netif_rx(skb) != NET_RX_SUCCESS)
		pr_err("netif_rx failed\n");

	return 0;
}

static int virtnet_tx(struct sk_buff *skb, struct net_device *dev)
{
	int i;
	struct vnet_priv *priv;
	priv = netdev_priv(dev);

	if (debug) {
		pr_err("virtnet TX(%d) data:\n", skb->len);
		for (i = 14 ; i < skb->len; i++)
			pr_cont(" %02x", skb->data[i]&0xff);
		printk("\n");
	}

	/* Logic for sending data */

	dev_kfree_skb(skb);

	return NETDEV_TX_OK;

// err:
// 	return NETDEV_TX_BUSY;
}


static int virtnet_change_mtu(struct net_device *dev, int new_mtu)
{
	// if (netif_running(dev)) {
	// 	pr_err("must be stopped to change its MTU\n");
	// 	return -EBUSY;
	// }

	if ((new_mtu < ETH_MIN_MTU) || (new_mtu > VNET_MAX_MTU))
		return -EINVAL;
	
	dev->mtu = new_mtu;
	netdev_update_features(dev);

	pr_err("%s MTU changed:%d\n", dev->name, new_mtu);

	return 0;
}

static int virtnet_open(struct net_device *net)
{
	netif_start_queue(net);

	return 0;
}

static int virtnet_stop(struct net_device *dev)
{
	netif_stop_queue(dev);

	return 0;
}

static struct net_device_ops vnetdev_ops = {
    .ndo_open = virtnet_open,
    .ndo_stop = virtnet_stop,
	.ndo_start_xmit = virtnet_tx,
	.ndo_change_mtu = virtnet_change_mtu,
};

static void virtnet_setup(struct net_device *dev)
{
	struct vnet_priv *priv;

	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(struct vnet_priv));
	priv->vnetdev = dev;

	ether_setup(dev);
	dev->flags           |= IFF_NOARP;
	dev->features        |= NETIF_F_HW_CSUM;
	dev->mtu = 9000;
	dev->max_mtu = VNET_MAX_MTU;
    dev->netdev_ops = &vnetdev_ops;
}

static int virtnet_probe(struct platform_device *pdev)
{
	int ret;
	struct vnet_priv *priv;
	struct net_device *vnetdev;

	pr_info("virtnet probe start\n");

	vnetdev = alloc_netdev(sizeof(struct vnet_priv),
				"virtnet%d", NET_NAME_UNKNOWN, virtnet_setup);
	if (vnetdev == NULL)
		return -ENOMEM;

	ret = register_netdev(vnetdev);
	if (ret)
		goto reg_err;

	priv = netdev_priv(vnetdev);
	platform_set_drvdata(pdev, priv);

	pr_info("virtnet probe complete\n");
	
	return 0;

reg_err:
	free_netdev(vnetdev);

	pr_err("virtnet probe failed\n");

	return -ENOMEM;
}

static int virtnet_remove(struct platform_device *pdev)
{
	struct vnet_priv *priv;

	priv = platform_get_drvdata(pdev);

	netif_stop_queue(priv->vnetdev);

	if (priv->vnetdev) {
		unregister_netdev(priv->vnetdev);
		free_netdev(priv->vnetdev);
	}

	pr_info("virtnet removed\n");

	return 0;
}

static const struct of_device_id virtnet_match[] = {
	{ .compatible = "virtnet"},
	{ }
};

static struct platform_driver virtnet_driver = {
	.probe  = virtnet_probe,
	.remove = virtnet_remove,
	.driver = {
		.name = "virtnet",
		.of_match_table = virtnet_match,
	},
};
module_platform_driver(virtnet_driver);

module_param(debug, int, S_IRUGO);
MODULE_PARM_DESC(debug, "print skb data");

MODULE_LICENSE("GPL v2");
MODULE_VERSION("v0.6");
MODULE_AUTHOR("grecod@163.com");
MODULE_DESCRIPTION("Virtual Network Driver");