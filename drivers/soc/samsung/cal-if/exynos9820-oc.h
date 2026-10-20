/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __EXYNOS9820_OC_H__
#define __EXYNOS9820_OC_H__

#define EXYNOS9820_M4_STOCK_KHZ	2730000U
#define EXYNOS9820_M4_OC_KHZ	2912000U
#define EXYNOS9820_M4_OC_HIGH_KHZ	3016000U

static inline int exynos9820_m4_oc_target_valid(unsigned int target_khz)
{
	return target_khz == EXYNOS9820_M4_OC_KHZ ||
		target_khz == EXYNOS9820_M4_OC_HIGH_KHZ;
}

/* Only select a complete, pre-existing firmware OPP; never invent a rail value. */
static inline int exynos9820_m4_oc_opp_valid(unsigned int stock_khz,
		unsigned int target_khz, unsigned int ect_khz,
		unsigned int firmware_khz,
		unsigned int stock_uv, unsigned int target_uv)
{
	unsigned int ceiling_uv = target_khz == EXYNOS9820_M4_OC_HIGH_KHZ ?
		1300000 : 1250000;

	return stock_khz == EXYNOS9820_M4_STOCK_KHZ &&
		exynos9820_m4_oc_target_valid(target_khz) &&
		ect_khz == target_khz &&
		firmware_khz == ect_khz && stock_uv >= 500000 &&
		stock_uv <= 1250000 && target_uv >= stock_uv &&
		target_uv <= ceiling_uv;
}

static inline int exynos9820_cluster_oc_opp_valid(unsigned int stock_khz,
		unsigned int target_khz, unsigned int ect_khz,
		unsigned int firmware_khz, unsigned int stock_uv,
		unsigned int target_uv)
{
	unsigned int ceiling_uv;

	if (stock_khz == 1950000 && target_khz == 2106000)
		ceiling_uv = 1175000;
	else if (stock_khz == 2314000 && target_khz == 2400000)
		ceiling_uv = 1050000;
	else
		return 0;

	return ect_khz == target_khz && firmware_khz == ect_khz &&
		stock_uv >= 500000 && stock_uv <= target_uv &&
		target_uv <= ceiling_uv;
}

/* Only the stock LITTLE startup band may use the validated OC ceiling. */
static inline unsigned int exynos9820_little_oc_cold_limit(int enabled,
		unsigned int stock_khz, unsigned int target_khz,
		unsigned int range, int temperature, unsigned int limit_khz)
{
	if (enabled && stock_khz == 1950000 && target_khz == 2106000 &&
	    range == 0 && temperature == 20 && limit_khz == stock_khz)
		return target_khz;
	return limit_khz;
}

/* 806 MHz is a separate same-voltage experiment: change only M (116 -> 124)
 * in the unused 754 MHz slot. The 26 MHz reference and P=4, S=0 stay fixed. */
static inline unsigned int exynos9820_gpu_oc_pms(unsigned int target_khz)
{
	if (target_khz == 754000)
		return 0x00740400;
	if (target_khz == 806000)
		return 0x007c0400;
	return 0;
}

/* Validate the observed original layout, not stability or ASV support. */
static inline int exynos9820_gpu_oc_slot_valid(unsigned int target_khz,
		unsigned int levels, unsigned int members, unsigned int plls,
		unsigned int top_khz, unsigned int top_uv,
		unsigned int top_index, unsigned int top_pms,
		unsigned int candidate_pms)
{
	return exynos9820_gpu_oc_pms(target_khz) && levels == 12 && members == 1 &&
		plls == 1 && top_khz == 702000 && top_uv >= 600000 &&
		top_uv <= 800000 && top_index == 1 &&
		top_pms == 0x006c0400 && candidate_pms == 0x00740400;
}

#endif
