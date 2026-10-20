#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/of.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/ect_parser.h>

#include "fvmap.h"
#include "cmucal.h"
#include "vclk.h"
#include "ra.h"
#include "exynos9820-oc.h"

#define FVMAP_SIZE		(SZ_8K)
#define STEP_UV			(6250)

void __iomem *fvmap_base;
void __iomem *sram_fvmap_base;

static int init_margin_table[MAX_MARGIN_ID];
static int volt_offset_percent = 0;
static int percent_margin_table[MAX_MARGIN_ID];

#if defined(CONFIG_SOC_EXYNOS9820) && !defined(CONFIG_SOC_EXYNOS9820_EVT0)
static bool cpucl2_oc_requested;
static unsigned int gpu_oc_requested;
static bool gpu_oc_enabled;
static unsigned int gpu_oc_voltage, gpu_oc_pll_id;
static const char *gpu_oc_status = "disabled";

static int __init get_gpu_oc(char *str)
{
	return kstrtouint(str, 0, &gpu_oc_requested);
}
early_param("exynos9820_gpu_oc", get_gpu_oc);
static bool cpucl2_oc_enabled;
static unsigned int cpucl2_oc_target = EXYNOS9820_M4_OC_KHZ;
static unsigned int cpucl2_stock_max;
static unsigned int cpucl2_candidate_uv;
static const char *cpucl2_oc_status = "not-initialised";

static int __init get_cpucl2_oc(char *str)
{
	unsigned int value;
	int ret = kstrtouint(str, 10, &value);

	if (ret)
		return ret;
	/* Keep the original 0/1 opt-in compatible with the first OC image. */
	if (value > 1 && !exynos9820_m4_oc_target_valid(value))
		return -EINVAL;
	cpucl2_oc_requested = value != 0;
	if (value > 1)
		cpucl2_oc_target = value;
	return 0;
}
early_param("exynos9820_cpu_oc", get_cpucl2_oc);

static bool cluster_oc_requested;
static int __init get_cluster_oc(char *str)
{
	return kstrtobool(str, &cluster_oc_requested);
}
early_param("exynos9820_cluster_oc", get_cluster_oc);

struct cluster_oc_state {
	unsigned int margin_id;
	const char *name;
	unsigned int target_khz;
	unsigned int stock_khz;
	unsigned int target_uv;
	bool enabled;
	const char *status;
};

static struct cluster_oc_state cluster_oc[] = {
	{ .margin_id = MARGIN_LIT, .name = "little", .target_khz = 2106000 },
	{ .margin_id = MARGIN_MID, .name = "mid", .target_khz = 2400000 },
};
#endif

static int __init get_mif_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_MIF] = volt;

	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_int_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_INT] = volt;

	return 0;
}
early_param("int", get_int_volt);

static int __init get_big_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_BIG] = volt;

	return 0;
}
early_param("big", get_big_volt);

static int __init get_mid_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_MID] = volt;

	return 0;
}
early_param("mid", get_mid_volt);

static int __init get_lit_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_LIT] = volt;

	return 0;
}
early_param("lit", get_lit_volt);

static int __init get_g3d_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_G3D] = volt;

	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_intcam_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_INTCAM] = volt;

	return 0;
}
early_param("intcam", get_intcam_volt);

static int __init get_cam_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_CAM] = volt;

	return 0;
}
early_param("cam", get_cam_volt);

static int __init get_disp_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_DISP] = volt;

	return 0;
}
early_param("disp", get_disp_volt);

static int __init get_g3dm_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_G3DM] = volt;

	return 0;
}
early_param("g3dm", get_g3dm_volt);

static int __init get_cp_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_CP] = volt;

	return 0;
}
early_param("cp", get_cp_volt);

static int __init get_fsys0_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_FSYS0] = volt;

	return 0;
}
early_param("fsys0", get_fsys0_volt);

static int __init get_aud_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_AUD] = volt;

	return 0;
}
early_param("aud", get_aud_volt);

static int __init get_iva_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_IVA] = volt;

	return 0;
}
early_param("iva", get_iva_volt);

static int __init get_score_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_SCORE] = volt;

	return 0;
}
early_param("score", get_score_volt);

static int __init get_npu_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_NPU] = volt;

	return 0;
}
early_param("npu", get_npu_volt);

static int __init get_mfc_volt(char *str)
{
	int volt;

	get_option(&str, &volt);
	init_margin_table[MARGIN_MFC] = volt;

	return 0;
}
early_param("mfc", get_mfc_volt);

static int __init get_percent_margin_volt(char *str)
{
	int percent;

	get_option(&str, &percent);
	volt_offset_percent = percent;

	return 0;
}
early_param("volt_offset_percent", get_percent_margin_volt);

int fvmap_set_raw_voltage_table(unsigned int id, int uV)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int num_of_lv;
	int idx, i;

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		fv_table->table[i].volt += uV;

	return 0;
}

int fvmap_get_voltage_table(unsigned int id, unsigned int *table)
{
	struct fvmap_header *fvmap_header = fvmap_base;
	struct rate_volt_header *fv_table;
	int idx, i;
	int num_of_lv;

	if (!IS_ACPM_VCLK(id))
		return 0;

	idx = GET_IDX(id);

	fvmap_header = fvmap_base;
	fv_table = fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table->table[i].volt;

	return num_of_lv;

}

int fvmap_get_raw_voltage_table(unsigned int id)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int idx, i;
	int num_of_lv;
	unsigned int table[20];

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table->table[i].volt;

	for (i = 0; i < num_of_lv; i++)
		printk("dvfs id : %d  %d Khz : %d uv\n", ACPM_VCLK_TYPE | id, fv_table->table[i].rate, table[i]);

	return 0;
}

static void check_percent_margin(struct rate_volt_header *head, unsigned int num_of_lv)
{
	int org_volt;
	int percent_volt;
	int i;

	if (!volt_offset_percent)
		return;

	for (i = 0; i < num_of_lv; i++) {
		org_volt = head->table[i].volt;
		percent_volt = org_volt * volt_offset_percent / 100;
		head->table[i].volt = org_volt + rounddown(percent_volt, STEP_UV);
	}
}

static int get_vclk_id_from_margin_id(int margin_id)
{
	int size = cmucal_get_list_size(ACPM_VCLK_TYPE);
	int i;
	struct vclk *vclk;

	for (i = 0; i < size; i++) {
		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);

		if (vclk->margin_id == margin_id)
			return i;
	}

	return -EINVAL;
}

#if defined(CONFIG_SOC_EXYNOS9820) && !defined(CONFIG_SOC_EXYNOS9820_EVT0)
static struct vclk *__init fvmap_validate_oc_opp(unsigned int margin_id,
		unsigned int target_khz, unsigned int *stock_khz,
		unsigned int *target_uv, const char **status)
{
	struct fvmap_header *header = fvmap_base;
	struct rate_volt_header *table;
	struct vclk *vclk;
	unsigned int stock_uv = 0;
	int id, level, target = -1;
	bool valid;

	if (!of_machine_is_compatible("samsung,exynos9820")) {
		*status = "unsupported-soc";
		return NULL;
	}

	id = get_vclk_id_from_margin_id(margin_id);
	if (id < 0) {
		*status = "missing-domain";
		return NULL;
	}

	vclk = cmucal_get_node(ACPM_VCLK_TYPE | id);
	if (!vclk) {
		*status = "missing-domain";
		return NULL;
	}
	*stock_khz = vclk->max_freq;
	if (!vclk->lut || vclk->num_rates != header[id].num_of_lv ||
	    header[id].o_ratevolt + header[id].num_of_lv *
	    sizeof(struct rate_volt) > FVMAP_SIZE) {
		*status = "inconsistent-firmware-table";
		return NULL;
	}

	table = fvmap_base + header[id].o_ratevolt;
	for (level = 0; level < header[id].num_of_lv; level++) {
		/* HIU addresses OPPs by level: ECT and firmware must agree. */
		if (vclk->lut[level].rate != table->table[level].rate) {
			*status = "ect-firmware-rate-mismatch";
			return NULL;
		}
		if (level && table->table[level - 1].rate <=
		    table->table[level].rate) {
			*status = "unordered-firmware-table";
			return NULL;
		}
		if (table->table[level].rate == *stock_khz)
			stock_uv = table->table[level].volt;
		if (table->table[level].rate == target_khz)
			target = level;
	}

	if (target < 0) {
		*status = "target-not-in-firmware";
		return NULL;
	}
	if (margin_id == MARGIN_BIG)
		valid = exynos9820_m4_oc_opp_valid(*stock_khz, target_khz,
			vclk->lut[target].rate, table->table[target].rate,
			stock_uv, table->table[target].volt);
	else
		valid = exynos9820_cluster_oc_opp_valid(*stock_khz, target_khz,
			vclk->lut[target].rate, table->table[target].rate,
			stock_uv, table->table[target].volt);
	if (!valid) {
		*status = "unsupported-stock-or-voltage";
		return NULL;
	}

	*target_uv = table->table[target].volt;
	*status = "available-disabled";
	return vclk;
}

static void __init fvmap_init_cpucl2_oc(void)
{
	struct vclk *vclk;

	vclk = fvmap_validate_oc_opp(MARGIN_BIG, cpucl2_oc_target,
		&cpucl2_stock_max, &cpucl2_candidate_uv, &cpucl2_oc_status);
	if (vclk && cpucl2_oc_requested) {
		/* ACPM/HIU retain the existing level, PLL and ASV voltage. */
		vclk->max_freq = cpucl2_oc_target;
		cpucl2_oc_enabled = true;
		cpucl2_oc_status = "enabled-experimental";
	}
	pr_info("cpucl2-oc: %s (requested=%u, stock=%u kHz, target=%u uV)\n",
		cpucl2_oc_status, cpucl2_oc_requested, cpucl2_stock_max,
		cpucl2_candidate_uv);
}

static void __init fvmap_init_cluster_oc(void)
{
	struct cluster_oc_state *state;
	struct vclk *vclk;
	int i;

	for (i = 0; i < ARRAY_SIZE(cluster_oc); i++) {
		state = &cluster_oc[i];
		vclk = fvmap_validate_oc_opp(state->margin_id, state->target_khz,
			&state->stock_khz, &state->target_uv, &state->status);
		if (vclk && cluster_oc_requested) {
			vclk->max_freq = state->target_khz;
			state->enabled = true;
			state->status = "enabled-experimental";
		}
		pr_info("cluster-oc: %s %s (%u -> %u kHz, %u uV)\n",
			state->name, state->status, state->stock_khz,
			state->target_khz, state->target_uv);
	}
}

bool exynos9820_cluster_oc_override(unsigned int id, unsigned int dt_max)
{
	struct cluster_oc_state *state;
	int i, index;

	for (i = 0; i < ARRAY_SIZE(cluster_oc); i++) {
		state = &cluster_oc[i];
		if (!state->enabled || dt_max != state->stock_khz)
			continue;
		index = get_vclk_id_from_margin_id(state->margin_id);
		if (index >= 0 && id == (ACPM_VCLK_TYPE | index))
			return true;
	}
	return false;
}

unsigned int exynos9820_gpu_oc_rate(unsigned int original_khz)
{
	return gpu_oc_enabled && original_khz == 702000 ? gpu_oc_requested : original_khz;
}

static void __init fvmap_init_gpu_oc(void)
{
	struct fvmap_header *headers = fvmap_base, *header;
	struct rate_volt_header *rates;
	struct ect_dvfs_domain *domain;
	struct ect_pll *ect_pll;
	struct cmucal_pll *pll;
	struct device_node *node;
	struct vclk *vclk;
	u32 pll_offset, pll_count, candidate, top, dt_max = 0, dt_first = 0;
	u32 value, old_index, target_pms;
	unsigned int id, level;

	if (!gpu_oc_requested)
		return;
	gpu_oc_status = "blocked-layout";
	if (init_margin_table[MARGIN_G3D] || volt_offset_percent)
		return;
	node = of_find_compatible_node(NULL, NULL, "arm,mali");
	if (!node)
		return;
	of_property_read_u32(node, "gpu_max_clock", &dt_max);
	of_property_read_u32_index(node, "gpu_dvfs_table", 0, &dt_first);
	of_node_put(node);
	if (dt_max != 702000 || dt_first != 702000)
		return;
	for (id = 0; id < cmucal_get_list_size(ACPM_VCLK_TYPE); id++) {
		vclk = cmucal_get_node(ACPM_VCLK_TYPE | id);
		if (!vclk || vclk->margin_id != MARGIN_G3D)
			continue;
		header = &headers[id];
		if (header->num_of_members != 1 || header->num_of_pll != 1 ||
		    header->num_of_lv != 12 || vclk->num_rates != 12 ||
		    vclk->num_list != 1 || vclk->max_freq != 702000 ||
		    header->o_members + sizeof(u16) > FVMAP_SIZE ||
		    header->o_tables + 12 > FVMAP_SIZE ||
		    header->o_ratevolt + 12 * sizeof(struct rate_volt) > FVMAP_SIZE)
			return;
		rates = fvmap_base + header->o_ratevolt;
		pll_offset = readw(sram_fvmap_base + header->o_members);
		if (pll_offset + offsetof(struct pll_header, pms) > FVMAP_SIZE)
			return;
		pll_count = readw(sram_fvmap_base + pll_offset +
			offsetof(struct pll_header, level));
		if (pll_count != 13 || pll_offset + offsetof(struct pll_header, pms) +
		    pll_count * sizeof(u32) > FVMAP_SIZE)
			return;
		candidate = readl(sram_fvmap_base + pll_offset + offsetof(struct pll_header, pms));
		top = readl(sram_fvmap_base + pll_offset + offsetof(struct pll_header, pms) + sizeof(u32));
		old_index = readb(sram_fvmap_base + header->o_tables);
		if (!exynos9820_gpu_oc_slot_valid(gpu_oc_requested,
		    header->num_of_lv, header->num_of_members, header->num_of_pll,
		    rates->table[0].rate, rates->table[0].volt, old_index, top, candidate))
			return;
		domain = ect_dvfs_get_domain(ect_get_block(BLOCK_DVFS), vclk->name);
		if (!domain || domain->num_of_level != 12 || !IS_PLL(vclk->list[0]))
			return;
		pll = cmucal_get_node(vclk->list[0]);
		ect_pll = ect_pll_get_pll(ect_get_block(BLOCK_PLL), "PLL_G3D");
		if (!pll || strcmp(pll->clk.name, "PLL_G3D") ||
		    !pll->rate_table || pll->rate_count != 13 ||
		    pll->rate_table[0].rate != 754000000 ||
		    pll->rate_table[0].mdiv != 116 || pll->rate_table[0].pdiv != 4 ||
		    pll->rate_table[0].sdiv != 0 || pll->rate_table[0].kdiv != 0 ||
		    !ect_pll || ect_pll->num_of_frequency != 13 ||
		    ect_pll->frequency_list[0].frequency != 754000000 ||
		    ect_pll->frequency_list[0].m != 116 ||
		    ect_pll->frequency_list[0].p != 4 ||
		    ect_pll->frequency_list[0].s != 0 ||
		    ect_pll->frequency_list[0].k != 0)
			return;
		for (level = 0; level < 12; level++) {
			value = readb(sram_fvmap_base + header->o_tables + level);
			if (value != level + 1 ||
			    vclk->lut[level].params[0] != value ||
			    vclk->lut[level].rate != rates->table[level].rate ||
			    domain->list_level[level].level != rates->table[level].rate)
				return;
		}
		target_pms = exynos9820_gpu_oc_pms(gpu_oc_requested);
		/* Replace one top slot in place; never grow packed SRAM tables or
		 * change voltage. 806 MHz additionally replaces the unused PLL PMS. */
		if (target_pms != candidate)
			writel(target_pms, sram_fvmap_base + pll_offset +
				offsetof(struct pll_header, pms));
		writel(gpu_oc_requested, sram_fvmap_base + header->o_ratevolt);
		writeb(0, sram_fvmap_base + header->o_tables);
		wmb();
		if (readl(sram_fvmap_base + pll_offset +
		    offsetof(struct pll_header, pms)) != target_pms ||
		    readl(sram_fvmap_base + header->o_ratevolt) != gpu_oc_requested ||
		    readb(sram_fvmap_base + header->o_tables) != 0) {
			writel(candidate, sram_fvmap_base + pll_offset +
				offsetof(struct pll_header, pms));
			writel(702000, sram_fvmap_base + header->o_ratevolt);
			writeb(old_index, sram_fvmap_base + header->o_tables);
			wmb();
			gpu_oc_status = "blocked-readback";
			return;
		}
		pll->rate_table[0].rate = gpu_oc_requested * 1000;
		pll->rate_table[0].mdiv = (target_pms >> 16) & 0x3ff;
		ect_pll->frequency_list[0].frequency = gpu_oc_requested * 1000;
		ect_pll->frequency_list[0].m = pll->rate_table[0].mdiv;
		domain->list_level[0].level = gpu_oc_requested;
		rates->table[0].rate = gpu_oc_requested;
		*((u8 *)fvmap_base + header->o_tables) = 0;
		vclk->lut[0].rate = gpu_oc_requested;
		vclk->lut[0].params[0] = 0;
		vclk->max_freq = gpu_oc_requested;
		gpu_oc_voltage = rates->table[0].volt;
		gpu_oc_pll_id = vclk->list[0];
		gpu_oc_enabled = true;
		gpu_oc_status = target_pms == candidate ? "experimental-same-voltage" :
			"experimental-new-pll-same-voltage";
		pr_info("exynos9820 GPU: firmware PLL slot 0 for %u kHz, PMS=0x%x at %u uV\n",
			gpu_oc_requested, target_pms, gpu_oc_voltage);
		return;
	}
}

unsigned int exynos9820_cluster_oc_thermal_limit(const char *zone,
		unsigned int range, int temperature, unsigned int limit_khz)
{
	struct cluster_oc_state *state;
	int i;

	if (!strcmp(zone, "G3D") && range == 0 && temperature == 20)
		return exynos9820_gpu_oc_rate(limit_khz);
	if (strcmp(zone, "LITTLE"))
		return limit_khz;
	for (i = 0; i < ARRAY_SIZE(cluster_oc); i++) {
		state = &cluster_oc[i];
		if (state->margin_id == MARGIN_LIT)
			return exynos9820_little_oc_cold_limit(state->enabled,
				state->stock_khz, state->target_khz,
				range, temperature, limit_khz);
	}
	return limit_khz;
}

static ssize_t cpucl2_oc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
		"status=%s\nrequested=%u\nenabled=%u\nstock_khz=%u\n"
		"target_khz=%u\ntarget_uv=%u\n",
		cpucl2_oc_status, cpucl2_oc_requested, cpucl2_oc_enabled,
		cpucl2_stock_max, cpucl2_oc_target, cpucl2_candidate_uv);
}

static ssize_t dvfs_tables_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct fvmap_header *header = fvmap_base;
	struct rate_volt_header *table;
	struct vclk *vclk;
	ssize_t len;
	int id, level;

	len = scnprintf(buf, PAGE_SIZE, "level rate_khz base_uv\n");
	for (id = 0; id < cmucal_get_list_size(ACPM_VCLK_TYPE); id++) {
		vclk = cmucal_get_node(ACPM_VCLK_TYPE | id);
		if (!vclk || (vclk->margin_id != MARGIN_BIG &&
		    vclk->margin_id != MARGIN_MID &&
		    vclk->margin_id != MARGIN_LIT &&
		    vclk->margin_id != MARGIN_G3D &&
		    vclk->margin_id != MARGIN_MIF &&
		    vclk->margin_id != MARGIN_INT))
			continue;
		if (header[id].o_ratevolt + header[id].num_of_lv *
		    sizeof(struct rate_volt) > FVMAP_SIZE)
			continue;
		table = fvmap_base + header[id].o_ratevolt;
		if (PAGE_SIZE - len < 96)
			return len;
		len += scnprintf(buf + len, PAGE_SIZE - len, "[%s] max_khz=%u\n",
			vclk->name, vclk->max_freq);
		for (level = 0; level < header[id].num_of_lv; level++) {
			if (PAGE_SIZE - len < 96)
				return len;
			len += scnprintf(buf + len, PAGE_SIZE - len,
				"%d %u %u\n", level, table->table[level].rate,
				table->table[level].volt);
		}
	}
	return len;
}

static ssize_t cluster_oc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct cluster_oc_state *state;
	ssize_t len = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(cluster_oc); i++) {
		state = &cluster_oc[i];
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"%s status=%s enabled=%u stock_khz=%u target_khz=%u target_uv=%u\n",
			state->name, state->status, state->enabled,
			state->stock_khz, state->target_khz, state->target_uv);
	}
	return len;
}

/* Read firmware's real parameter indices/PMS, not only the Linux rate labels. */
static ssize_t firmware_clocks_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct fvmap_header *headers = fvmap_base;
	struct fvmap_header *header;
	struct vclk *vclk;
	ssize_t len = 0;
	unsigned int id, member, level, offset, pll_offset, pll_levels;
	unsigned int params, index, pms;
	struct cmucal_clk *clk;

	for (id = 0; id < cmucal_get_list_size(ACPM_VCLK_TYPE); id++) {
		vclk = cmucal_get_node(ACPM_VCLK_TYPE | id);
		if (!vclk || (vclk->margin_id != MARGIN_G3D &&
		    vclk->margin_id != MARGIN_MIF))
			continue;
		header = &headers[id];
		if (header->num_of_members != vclk->num_list ||
		    header->num_of_lv != vclk->num_rates ||
		    header->o_members + header->num_of_members * sizeof(u16) > FVMAP_SIZE ||
		    header->o_tables + header->num_of_members * header->num_of_lv > FVMAP_SIZE)
			return -EINVAL;
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[%s] id=0x%x levels=%u members=%u plls=%u ratevolt=%u tables=%u\n",
			vclk->name, vclk->id, header->num_of_lv,
			header->num_of_members, header->num_of_pll,
			header->o_ratevolt, header->o_tables);
		for (member = 0; member < header->num_of_pll; member++) {
			if (PAGE_SIZE - len < 160)
				return len;
			offset = header->o_members + member * sizeof(u16);
			pll_offset = readw(sram_fvmap_base + offset);
			if (pll_offset + offsetof(struct pll_header, pms) > FVMAP_SIZE)
				return -EINVAL;
			pll_levels = readw(sram_fvmap_base + pll_offset +
				offsetof(struct pll_header, level));
			if (pll_levels > 64 || pll_offset + offsetof(struct pll_header, pms) +
			    pll_levels * sizeof(u32) > FVMAP_SIZE)
				return -EINVAL;
			clk = cmucal_get_node(vclk->list[member]);
			if (!clk || !IS_PLL(clk->id))
				return -EINVAL;
			len += scnprintf(buf + len, PAGE_SIZE - len,
				"pll=%s member=%u offset=%u entries=%u actual_hz=%u\n",
				clk->name, member, pll_offset, pll_levels,
				ra_recalc_rate(clk->id));
			for (level = 0; level < pll_levels; level++) {
				if (PAGE_SIZE - len < 80)
					return len;
				pms = readl(sram_fvmap_base + pll_offset +
					offsetof(struct pll_header, pms) + level * sizeof(u32));
				len += scnprintf(buf + len, PAGE_SIZE - len,
					"pms[%u]=0x%08x\n", level, pms);
			}
		}
		for (level = 0; level < header->num_of_lv; level++) {
			if (PAGE_SIZE - len < 200)
				return len;
			len += scnprintf(buf + len, PAGE_SIZE - len,
				"level=%u rate_khz=%u params=", level, vclk->lut[level].rate);
			for (member = 0; member < header->num_of_members; member++) {
				params = header->o_tables + level * header->num_of_members + member;
				index = readb(sram_fvmap_base + params);
				if (PAGE_SIZE - len < 8)
					return len;
				len += scnprintf(buf + len, PAGE_SIZE - len, "%u,", index);
			}
			len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
		}
	}
	return len;
}

static struct kobj_attribute cpucl2_oc_attr =
	__ATTR(cpucl2_oc, 0444, cpucl2_oc_show, NULL);
static ssize_t gpu_oc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
		"status=%s\nrequested_khz=%u\nenabled=%u\ntop_voltage_uv=%u\n"
		"actual_pll_hz=%u\n",
		gpu_oc_status, gpu_oc_requested, gpu_oc_enabled, gpu_oc_voltage,
		gpu_oc_enabled ? ra_recalc_rate(gpu_oc_pll_id) : 0);
}
static struct kobj_attribute gpu_oc_attr =
	__ATTR(gpu_oc, 0400, gpu_oc_show, NULL);

static ssize_t characterisation_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ect_dvfs_domain *dvfs;
	struct ect_voltage_domain *asv;
	struct ect_timing_param_header *timing = ect_get_block(BLOCK_TIMING_PARAM);
	struct ect_new_timing_param_header *new_timing = ect_get_block(BLOCK_NEW_TIMING_PARAM);
	struct ect_gen_param_header *general = ect_get_block(BLOCK_GEN_PARAM);
	const char *names[] = { "dvfs_mif", "dvfs_g3d" };
	ssize_t len = 0;
	unsigned int maximum;
	int domain, level;

	for (domain = 0; domain < ARRAY_SIZE(names); domain++) {
		dvfs = ect_dvfs_get_domain(ect_get_block(BLOCK_DVFS), (char *)names[domain]);
		asv = ect_asv_get_domain(ect_get_block(BLOCK_ASV), (char *)names[domain]);
		len += scnprintf(buf + len, PAGE_SIZE - len, "[%s]\nECT_DVFS_khz=", names[domain]);
		if (dvfs)
			for (level = 0; level < dvfs->num_of_level; level++)
				len += scnprintf(buf + len, PAGE_SIZE - len, "%u,", dvfs->list_level[level].level);
		maximum = 0;
		if (asv)
			for (level = 0; level < asv->num_of_level; level++)
				maximum = max(maximum, asv->level_list[level]);
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"\nECT_ASV_max_native=%u levels=%d tables=%d\n", maximum,
			asv ? asv->num_of_level : 0, asv ? asv->num_of_table : 0);
	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "TIMING_sets=%d\nNEW_TIMING_sets=%d\n",
		timing ? timing->num_of_size : 0, new_timing ? new_timing->num_of_size : 0);
	if (timing)
		for (domain = 0; domain < timing->num_of_size && PAGE_SIZE - len > 128; domain++)
			len += scnprintf(buf + len, PAGE_SIZE - len,
				"TIMING key=0x%llx levels=%d parameters=%d\n",
				timing->size_list[domain].parameter_key,
				timing->size_list[domain].num_of_level,
				timing->size_list[domain].num_of_timing_param);
	if (new_timing)
		for (domain = 0; domain < new_timing->num_of_size && PAGE_SIZE - len > 128; domain++)
			len += scnprintf(buf + len, PAGE_SIZE - len,
				"NEW_TIMING key=0x%llx levels=%d parameters=%d\n",
				new_timing->size_list[domain].parameter_key,
				new_timing->size_list[domain].num_of_level,
				new_timing->size_list[domain].num_of_timing_param);
	if (general)
		for (domain = 0; domain < general->num_of_table && PAGE_SIZE - len > 128; domain++)
			len += scnprintf(buf + len, PAGE_SIZE - len,
				"GEN name=%s rows=%d cols=%d\n", general->table_list[domain].table_name,
				general->table_list[domain].num_of_row, general->table_list[domain].num_of_col);
	return len;
}
static struct kobj_attribute characterisation_attr =
	__ATTR(characterisation, 0400, characterisation_show, NULL);

static ssize_t timing_rows_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ect_new_timing_param_header *header = ect_get_block(BLOCK_NEW_TIMING_PARAM);
	struct ect_new_timing_param_size *table;
	ssize_t len = 0;
	int row, column, set;
	bool main = !strcmp(attr->attr.name, "mif_timing_rows");
	bool dram = !strcmp(attr->attr.name, "mif_dram_rows");

	if (!header)
		return -ENODATA;
	for (set = 0; set < header->num_of_size; set++) {
		table = &header->size_list[set];
		if (main ? table->parameter_key != 0x11111111 : dram ?
		    table->parameter_key != 0xac1 :
		    (table->parameter_key != 0xaa1 && table->parameter_key != 0xaa2))
			continue;
		if (table->num_of_timing_param < 1 || table->num_of_timing_param > 64)
			return -EINVAL;
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"key=0x%llx mode=%u levels=%d parameters=%d\n",
			table->parameter_key, table->mode, table->num_of_level,
			table->num_of_timing_param);
		for (row = 0; row < table->num_of_level; row++) {
			if (PAGE_SIZE - len < table->num_of_timing_param * 18 + 20)
				return len;
			len += scnprintf(buf + len, PAGE_SIZE - len, "row=%d ", row);
			for (column = 0; column < table->num_of_timing_param; column++) {
				if (table->mode == e_mode_extend_value)
					len += scnprintf(buf + len, PAGE_SIZE - len, "%016llx,",
						ect_read_value64(table->timing_parameter,
						row * table->num_of_timing_param + column));
				else
					len += scnprintf(buf + len, PAGE_SIZE - len, "%08x,",
						table->timing_parameter[row * table->num_of_timing_param + column]);
			}
			len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
		}
	}
	return len;
}
static struct kobj_attribute mif_timing_attr =
	__ATTR(mif_timing_rows, 0400, timing_rows_show, NULL);
static struct kobj_attribute mif_phy_attr =
	__ATTR(mif_phy_rows, 0400, timing_rows_show, NULL);
static struct kobj_attribute mif_dram_attr =
	__ATTR(mif_dram_rows, 0400, timing_rows_show, NULL);
static struct kobj_attribute dvfs_tables_attr =
	__ATTR(dvfs_tables, 0400, dvfs_tables_show, NULL);
static struct kobj_attribute cluster_oc_attr =
	__ATTR(cluster_oc, 0444, cluster_oc_show, NULL);
static struct kobj_attribute firmware_clocks_attr =
	__ATTR(firmware_clocks, 0400, firmware_clocks_show, NULL);

static struct attribute *oc_attrs[] = {
	&cpucl2_oc_attr.attr,
	&dvfs_tables_attr.attr,
	&cluster_oc_attr.attr,
	&firmware_clocks_attr.attr,
	&gpu_oc_attr.attr,
	&characterisation_attr.attr,
	&mif_timing_attr.attr,
	&mif_phy_attr.attr,
	&mif_dram_attr.attr,
	NULL,
};

static const struct attribute_group oc_group = {
	.name = "exynos9820_oc",
	.attrs = oc_attrs,
};
#endif

#define attr_percent(margin_id, type)								\
static ssize_t show_##type##_percent								\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)					\
{												\
	return snprintf(buf, PAGE_SIZE, "%d\n", percent_margin_table[margin_id]);		\
}												\
												\
static ssize_t store_##type##_percent								\
(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)		\
{												\
	int input, vclk_id;									\
												\
	if (!sscanf(buf, "%d", &input))								\
		return -EINVAL;									\
												\
	if (input < -100 || input > 100)							\
		return -EINVAL;									\
												\
	vclk_id = get_vclk_id_from_margin_id(margin_id);					\
	if (vclk_id == -EINVAL)									\
		return vclk_id;									\
	percent_margin_table[margin_id] = input;						\
	cal_dfs_set_volt_margin(vclk_id | ACPM_VCLK_TYPE, input);				\
												\
	return count;										\
}												\
												\
static struct kobj_attribute type##_percent =							\
__ATTR(type##_percent, 0600,									\
	show_##type##_percent, store_##type##_percent)

attr_percent(MARGIN_MIF, mif_margin);
attr_percent(MARGIN_INT, int_margin);
attr_percent(MARGIN_BIG, big_margin);
attr_percent(MARGIN_MID, mid_margin);
attr_percent(MARGIN_LIT, lit_margin);
attr_percent(MARGIN_G3D, g3d_margin);
attr_percent(MARGIN_INTCAM, intcam_margin);
attr_percent(MARGIN_CAM, cam_margin);
attr_percent(MARGIN_DISP, disp_margin);
attr_percent(MARGIN_CP, cp_margin);
attr_percent(MARGIN_FSYS0, fsys0_margin);
attr_percent(MARGIN_AUD, aud_margin);
attr_percent(MARGIN_IVA, iva_margin);
attr_percent(MARGIN_SCORE, score_margin);
attr_percent(MARGIN_NPU, npu_margin);
attr_percent(MARGIN_MFC, mfc_margin);

static struct attribute *percent_margin_attrs[] = {
	&mif_margin_percent.attr,
	&int_margin_percent.attr,
	&big_margin_percent.attr,
	&mid_margin_percent.attr,
	&lit_margin_percent.attr,
	&g3d_margin_percent.attr,
	&intcam_margin_percent.attr,
	&cam_margin_percent.attr,
	&disp_margin_percent.attr,
	&cp_margin_percent.attr,
	&fsys0_margin_percent.attr,
	&aud_margin_percent.attr,
	&iva_margin_percent.attr,
	&score_margin_percent.attr,
	&npu_margin_percent.attr,
	&mfc_margin_percent.attr,
	NULL,
};

static const struct attribute_group percent_margin_group = {
	.attrs = percent_margin_attrs,
};

static void fvmap_copy_from_sram(void __iomem *map_base, void __iomem *sram_base)
{
	volatile struct fvmap_header *fvmap_header, *header;
	struct rate_volt_header *old, *new;
	struct dvfs_table *old_param, *new_param;
	struct clocks *clks;
	struct pll_header *plls;
	struct vclk *vclk;
	unsigned int member_addr;
	unsigned int blk_idx, param_idx;
	int size, margin;
	int i, j, k;

	fvmap_header = map_base;
	header = sram_base;

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);

	for (i = 0; i < size; i++) {
		/* load fvmap info */
		fvmap_header[i].dvfs_type = header[i].dvfs_type;
		fvmap_header[i].num_of_lv = header[i].num_of_lv;
		fvmap_header[i].num_of_members = header[i].num_of_members;
		fvmap_header[i].num_of_pll = header[i].num_of_pll;
		fvmap_header[i].num_of_mux = header[i].num_of_mux;
		fvmap_header[i].num_of_div = header[i].num_of_div;
		fvmap_header[i].gearratio = header[i].gearratio;
		fvmap_header[i].init_lv = header[i].init_lv;
		fvmap_header[i].num_of_gate = header[i].num_of_gate;
		fvmap_header[i].reserved[0] = header[i].reserved[0];
		fvmap_header[i].reserved[1] = header[i].reserved[1];
		fvmap_header[i].block_addr[0] = header[i].block_addr[0];
		fvmap_header[i].block_addr[1] = header[i].block_addr[1];
		fvmap_header[i].block_addr[2] = header[i].block_addr[2];
		fvmap_header[i].o_members = header[i].o_members;
		fvmap_header[i].o_ratevolt = header[i].o_ratevolt;
		fvmap_header[i].o_tables = header[i].o_tables;

		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		if (vclk == NULL)
			continue;
		pr_info("dvfs_type : %s - id : %x\n",
			vclk->name, fvmap_header[i].dvfs_type);
		pr_info("  num_of_lv      : %d\n", fvmap_header[i].num_of_lv);
		pr_info("  num_of_members : %d\n", fvmap_header[i].num_of_members);

		old = sram_base + fvmap_header[i].o_ratevolt;
		new = map_base + fvmap_header[i].o_ratevolt;

		check_percent_margin(old, fvmap_header[i].num_of_lv);

		margin = init_margin_table[vclk->margin_id];
		if (margin)
			cal_dfs_set_volt_margin(i | ACPM_VCLK_TYPE, margin);

		for (j = 0; j < fvmap_header[i].num_of_members; j++) {
			clks = sram_base + fvmap_header[i].o_members;

			if (j < fvmap_header[i].num_of_pll) {
				plls = sram_base + clks->addr[j];
				member_addr = plls->addr - 0x90000000;
			} else {

				member_addr = (clks->addr[j] & ~0x3) & 0xffff;
				blk_idx = clks->addr[j] & 0x3;

				if (blk_idx < BLOCK_ADDR_SIZE)
					member_addr |= ((fvmap_header[i].block_addr[blk_idx]) << 16) - 0x90000000;
				else
					pr_err("[%s] blk_idx %u is out of range for block_addr\n", __func__, blk_idx);
			}


			vclk->list[j] = cmucal_get_id_by_addr(member_addr);

			if (vclk->list[j] == INVALID_CLK_ID)
				pr_info("  Invalid addr :0x%x\n", member_addr);
			else
				pr_info("  DVFS CMU addr:0x%x\n", member_addr);
		}

		for (j = 0; j < fvmap_header[i].num_of_lv; j++) {
			new->table[j].rate = old->table[j].rate;
			new->table[j].volt = old->table[j].volt;
			pr_info("  lv : [%7d], volt = %d uV (%d %%) \n",
				new->table[j].rate, new->table[j].volt,
				volt_offset_percent);
		}

		old_param = sram_base + fvmap_header[i].o_tables;
		new_param = map_base + fvmap_header[i].o_tables;
		for (j = 0; j < fvmap_header[i].num_of_lv; j++) {
			for (k = 0; k < fvmap_header[i].num_of_members; k++) {
				param_idx = fvmap_header[i].num_of_members * j + k;
				new_param->val[param_idx] = old_param->val[param_idx];
				if (vclk->lut[j].params[k] != new_param->val[param_idx]) {
					vclk->lut[j].params[k] = new_param->val[param_idx];
					pr_info("Mis-match %s[%d][%d] : %d %d\n",
						vclk->name, j, k,
						vclk->lut[j].params[k],
						new_param->val[param_idx]);
				}
			}
		}
	}
}

int fvmap_init(void __iomem *sram_base)
{
	void __iomem *map_base;
	struct kobject *kobj;

	map_base = kzalloc(FVMAP_SIZE, GFP_KERNEL);

	fvmap_base = map_base;
	sram_fvmap_base = sram_base;
	pr_info("%s:fvmap initialize %p\n", __func__, sram_base);
	fvmap_copy_from_sram(map_base, sram_base);

#if defined(CONFIG_SOC_EXYNOS9820) && !defined(CONFIG_SOC_EXYNOS9820_EVT0)
	fvmap_init_cpucl2_oc();
	fvmap_init_cluster_oc();
	fvmap_init_gpu_oc();
	if (sysfs_create_group(power_kobj, &oc_group))
		pr_err("Fail to create exynos9820_oc group\n");
#endif

	/* percent margin for each doamin at runtime */
	kobj = kobject_create_and_add("percent_margin", power_kobj);
	if (!kobj)
		pr_err("Fail to create percent_margin kboject\n");

	if (sysfs_create_group(kobj, &percent_margin_group))
		pr_err("Fail to create percent_margin group\n");

	return 0;
}
