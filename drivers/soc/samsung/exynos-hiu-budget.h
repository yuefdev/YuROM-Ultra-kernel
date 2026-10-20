/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __EXYNOS_HIU_BUDGET_H__
#define __EXYNOS_HIU_BUDGET_H__

/* Budget units are firmware-defined, not watts. Limit the optional uplift. */
static inline unsigned int exynos_hiu_budget_max(unsigned int stock)
{
	unsigned int maximum;

	if (!stock || stock > 255)
		return 0;
	maximum = stock + stock / 10;
	return maximum > 255 ? 255 : maximum;
}

static inline int exynos_hiu_budget_valid(unsigned int stock, unsigned int value)
{
	return stock && stock <= 255 && value >= stock &&
		value <= exynos_hiu_budget_max(stock);
}

#endif
