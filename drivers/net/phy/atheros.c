// SPDX-License-Identifier: GPL-2.0+
/*
 * Atheros PHY drivers
 *
 * Copyright 2011, 2013 Freescale Semiconductor, Inc.
 * author Andy Fleming
 * Copyright (c) 2019 Michael Walle <michael@walle.cc>
 */
#include <common.h>
#include <phy.h>

#define AR803x_PHY_DEBUG_ADDR_REG	0x1d
#define AR803x_PHY_DEBUG_DATA_REG	0x1e

/* Debug registers */
#define AR803x_DEBUG_REG_0		0x0
#define AR803x_RGMII_RX_CLK_DLY		BIT(15)

#define AR803x_DEBUG_REG_5		0x5
#define AR803x_RGMII_TX_CLK_DLY		BIT(8)

#define AR803x_DEBUG_REG_1F		0x1f
#define AR803x_PLL_ON			BIT(2)
#define AR803x_RGMII_1V8		BIT(3)

/* MMD registers */
#define AR803x_MMD7_CLK25M		0x8016
#define AR803x_CLK_OUT_25MHZ_XTAL	(0 << 2)
#define AR803x_CLK_OUT_25MHZ_DSP	(1 << 2)
#define AR803x_CLK_OUT_50MHZ_PLL	(2 << 2)
#define AR803x_CLK_OUT_50MHZ_DSP	(3 << 2)
#define AR803x_CLK_OUT_62_5MHZ_PLL	(4 << 2)
#define AR803x_CLK_OUT_62_5MHZ_DSP	(5 << 2)
#define AR803x_CLK_OUT_125MHZ_PLL	(6 << 2)
#define AR803x_CLK_OUT_125MHZ_DSP	(7 << 2)
#define AR803x_CLK_OUT_MASK		(7 << 2)

#define AR803x_CLK_OUT_STRENGTH_FULL	(0 << 6)
#define AR803x_CLK_OUT_STRENGTH_HALF	(1 << 6)
#define AR803x_CLK_OUT_STRENGTH_QUARTER	(2 << 6)
#define AR803x_CLK_OUT_STRENGTH_MASK	(3 << 6)

struct ar803x_priv {
	int flags;
#define AR803x_FLAG_KEEP_PLL_ENABLED	BIT(0) /* don't turn off internal PLL */
#define AR803x_FLAG_RGMII_1V8		BIT(1) /* use 1.8V RGMII I/O voltage */
	u16 clk_25m_reg;
	u16 clk_25m_mask;
};

static int ar803x_debug_reg_read(struct phy_device *phydev, u16 reg)
{
	int ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, AR803x_PHY_DEBUG_ADDR_REG,
			reg);
	if (ret < 0)
		return ret;

	return phy_read(phydev, MDIO_DEVAD_NONE, AR803x_PHY_DEBUG_DATA_REG);
}

static int ar803x_debug_reg_write(struct phy_device *phydev, u16 reg, u16 val)
{
	int ret;

	ret = phy_write(phydev, MDIO_DEVAD_NONE, AR803x_PHY_DEBUG_ADDR_REG,
			reg);
	if (ret < 0)
		return ret;

	return phy_write(phydev, MDIO_DEVAD_NONE, AR803x_PHY_DEBUG_DATA_REG,
			 val);
}

static int ar803x_debug_reg_mask(struct phy_device *phydev, u16 reg,
				 u16 clear, u16 set)
{
	int val;

	val = ar803x_debug_reg_read(phydev, reg);
	if (val < 0)
		return val;

	val &= 0xffff;
	val &= ~clear;
	val |= set;

	return phy_write(phydev, MDIO_DEVAD_NONE, AR803x_PHY_DEBUG_DATA_REG,
			 val);
}

static int ar8021_config(struct phy_device *phydev)
{
	phy_write(phydev, MDIO_DEVAD_NONE, 0x00, 0x1200);
	ar803x_debug_reg_write(phydev, AR803x_DEBUG_REG_5, 0x3D47);

	phydev->supported = phydev->drv->features;
	return 0;
}

static int ar803x_delay_config(struct phy_device *phydev)
{
	int ret;

	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_ID) {
		ret = ar803x_debug_reg_mask(phydev, AR803x_DEBUG_REG_5,
					    0, AR803x_RGMII_TX_CLK_DLY);
		if (ret < 0)
			return ret;
	} else {
		ret = ar803x_debug_reg_mask(phydev, AR803x_DEBUG_REG_5,
					    AR803x_RGMII_TX_CLK_DLY, 0);
		if (ret < 0)
			return ret;
	}

	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_RXID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_ID) {
		ret = ar803x_debug_reg_mask(phydev, AR803x_DEBUG_REG_0,
					    0, AR803x_RGMII_RX_CLK_DLY);
		if (ret < 0)
			return ret;
	} else {
		ret = ar803x_debug_reg_mask(phydev, AR803x_DEBUG_REG_0,
					    AR803x_RGMII_RX_CLK_DLY, 0);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ar803x_regs_config(struct phy_device *phydev)
{
	struct ar803x_priv *priv = phydev->priv;
	u16 set = 0, clear = 0;
	int val;
	int ret;

	/* no configuration available */
	if (!priv)
		return 0;

	if (priv->flags & AR803x_FLAG_KEEP_PLL_ENABLED)
		set |= AR803x_PLL_ON;
	else
		clear |= AR803x_PLL_ON;

	if (priv->flags & AR803x_FLAG_RGMII_1V8)
		set |= AR803x_RGMII_1V8;
	else
		clear |= AR803x_RGMII_1V8;

	ret = ar803x_debug_reg_mask(phydev, AR803x_DEBUG_REG_1F, clear, set);
	if (ret < 0)
		return ret;

	/* save the write access if the mask is empty */
	if (priv->clk_25m_mask) {
		val = phy_read_mmd(phydev, 7, AR803x_MMD7_CLK25M);
		if (val < 0)
			return val;
		val &= ~priv->clk_25m_mask;
		val |= priv->clk_25m_reg;
		ret = phy_write_mmd(phydev, 7, AR803x_MMD7_CLK25M, val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ar803x_of_init(struct phy_device *phydev)
{
#if defined(CONFIG_DM_ETH)
	struct ar803x_priv *priv;
	ofnode node;
	const char *strength;
	u32 freq;

	priv = malloc(sizeof(*priv));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));

	phydev->priv = priv;

	node = phy_get_ofnode(phydev);
	if (!ofnode_valid(node))
		return -EINVAL;

	debug("%s: found PHY node: %s\n", __func__, ofnode_get_name(node));

	if (ofnode_read_bool(node, "atheros,keep-pll-enabled"))
		priv->flags |= AR803x_FLAG_KEEP_PLL_ENABLED;
	if (ofnode_read_bool(node, "atheros,rgmii-io-1v8"))
		priv->flags |= AR803x_FLAG_RGMII_1V8;

	/*
	 * Get the CLK_OUT frequency from the device tree. Only XTAL and PLL
	 * sources are supported right now. There is also the possibilty to use
	 * the DSP as frequency reference, this is used for synchronous
	 * ethernet.
	 */
	freq = ofnode_read_u32_default(node, "atheros,clk-out-frequency", 0);
	if (freq) {
		priv->clk_25m_mask |= AR803x_CLK_OUT_MASK;
		if (freq == 25000000) {
			priv->clk_25m_reg |= AR803x_CLK_OUT_25MHZ_XTAL;
		} else if (freq == 50000000) {
			priv->clk_25m_reg |= AR803x_CLK_OUT_50MHZ_PLL;
		} else if (freq == 62500000) {
			priv->clk_25m_reg |= AR803x_CLK_OUT_62_5MHZ_PLL;
		} else if (freq == 125000000) {
			priv->clk_25m_reg |= AR803x_CLK_OUT_125MHZ_PLL;
		} else {
			dev_err(phydev->dev,
				"invalid atheros,clk-out-frequency\n");
			free(priv);
			return -EINVAL;
		}
	}

	strength = ofnode_read_string(node, "atheros,clk-out-strength");
	if (strength) {
		priv->clk_25m_mask |= AR803x_CLK_OUT_STRENGTH_MASK;
		if (!strcmp(strength, "full")) {
			priv->clk_25m_reg |= AR803x_CLK_OUT_STRENGTH_FULL;
		} else if (!strcmp(strength, "half")) {
			priv->clk_25m_reg |= AR803x_CLK_OUT_STRENGTH_HALF;
		} else if (!strcmp(strength, "quarter")) {
			priv->clk_25m_reg |= AR803x_CLK_OUT_STRENGTH_QUARTER;
		} else {
			dev_err(phydev->dev, "invalid atheros,strength\n");
			free(priv);
			return -EINVAL;
		}
	}

	debug("%s: flags=%x clk_25m_reg=%04x\n", __func__, priv->flags,
	      priv->clk_25m_reg);
#endif

	return 0;
}

static int ar803x_config(struct phy_device *phydev)
{
	int ret;

	ret = ar803x_of_init(phydev);
	if (ret < 0)
		return ret;

	ret = ar803x_delay_config(phydev);
	if (ret < 0)
		return ret;

	ret = ar803x_regs_config(phydev);
	if (ret < 0)
		return ret;

	phydev->supported = phydev->drv->features;

	genphy_config_aneg(phydev);
	genphy_restart_aneg(phydev);

	return 0;
}

static struct phy_driver AR8021_driver =  {
	.name = "AR8021",
	.uid = 0x4dd040,
	.mask = 0x4ffff0,
	.features = PHY_GBIT_FEATURES,
	.config = ar8021_config,
	.startup = genphy_startup,
	.shutdown = genphy_shutdown,
};

static struct phy_driver AR8031_driver =  {
	.name = "AR8031/AR8033",
	.uid = 0x4dd074,
	.mask = 0xffffffef,
	.features = PHY_GBIT_FEATURES,
	.config = ar803x_config,
	.startup = genphy_startup,
	.shutdown = genphy_shutdown,
};

static struct phy_driver AR8035_driver =  {
	.name = "AR8035",
	.uid = 0x4dd072,
	.mask = 0xffffffef,
	.features = PHY_GBIT_FEATURES,
	.config = ar803x_config,
	.startup = genphy_startup,
	.shutdown = genphy_shutdown,
};

int phy_atheros_init(void)
{
	phy_register(&AR8021_driver);
	phy_register(&AR8031_driver);
	phy_register(&AR8035_driver);

	return 0;
}
