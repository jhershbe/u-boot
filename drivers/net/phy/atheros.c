// SPDX-License-Identifier: GPL-2.0+
/*
 * Atheros PHY drivers
 *
 * Copyright 2011, 2013 Freescale Semiconductor, Inc.
 * author Andy Fleming
 */
#include <common.h>
#include <phy.h>

#define AR803x_PHY_DEBUG_ADDR_REG	0x1d
#define AR803x_PHY_DEBUG_DATA_REG	0x1e

#define AR803x_DEBUG_REG_5		0x5
#define AR803x_RGMII_TX_CLK_DLY		BIT(8)

#define AR803x_DEBUG_REG_0		0x0
#define AR803x_RGMII_RX_CLK_DLY		BIT(15)

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
	}

	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_RXID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_ID) {
		ret = ar803x_debug_reg_mask(phydev, AR803x_DEBUG_REG_0,
					    0, AR803x_RGMII_RX_CLK_DLY);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ar8031_config(struct phy_device *phydev)
{
	int ret;

	ret = ar803x_delay_config(phydev);
	if (ret < 0)
		return ret;

	phydev->supported = phydev->drv->features;

	genphy_config_aneg(phydev);
	genphy_restart_aneg(phydev);

	return 0;
}

static int ar8035_config(struct phy_device *phydev)
{
	int ret;

	ret = phy_read_mmd(phydev, 7, 0x8016);
	if (ret < 0)
		return ret;
	ret |= 0x0018;
	phy_write_mmd(phydev, 7, 0x8016, ret);

	ret = ar803x_delay_config(phydev);
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
	.config = ar8031_config,
	.startup = genphy_startup,
	.shutdown = genphy_shutdown,
};

static struct phy_driver AR8035_driver =  {
	.name = "AR8035",
	.uid = 0x4dd072,
	.mask = 0xffffffef,
	.features = PHY_GBIT_FEATURES,
	.config = ar8035_config,
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
