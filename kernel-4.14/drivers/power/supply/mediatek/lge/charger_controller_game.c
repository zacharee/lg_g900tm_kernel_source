#define pr_fmt(fmt) "[CC][GAME]%s: " fmt, __func__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>

#include "charger_controller.h"

enum {
	CC_GAME_LOAD_NONE,
	CC_GAME_LOAD_LIGHT,
	CC_GAME_LOAD_HEAVY,
};

static int game_mode = 0;
static int game_load = CC_GAME_LOAD_NONE;
static struct timespec start;

static int set_game_mode(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	struct chgctrl_game *game = &chip->game;
	int ret;

	if (!chip)
		return -ENODEV;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("failed to set game mode (%d)\n", ret);
		return ret;
	}

	if (!game->enabled)
		return 0;

	if (!game_mode) {
		if (game_load == CC_GAME_LOAD_NONE)
			return 0;

		/* to deactive immediatly, cancel scheduled work */
		cancel_delayed_work(&game->dwork);

		goto game_start;
	}

	if (game_load != CC_GAME_LOAD_NONE)
		return 0;

game_start:
	schedule_delayed_work(&game->dwork, 0);

	return 0;
}
module_param_call(game_mode, set_game_mode,
		  param_get_int, &game_mode, CC_RW_PERM);

extern bool mtk_get_gpu_loading(unsigned int *pLoading);
static void chgctrl_game_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct chgctrl_game *game = container_of(dwork, struct chgctrl_game,
			dwork);
	struct chgctrl *chip = container_of(game, struct chgctrl, game);

	unsigned int load = game->light_load;
	int fcc = game->fcc;
	int icl = game->icl;
	struct timespec now, diff;

	if (!game_mode) {
		chgctrl_vote(&chip->fcc, FCC_VOTER_GAME, -1);
		chgctrl_vote(&chip->icl, ICL_VOTER_GAME, -1);
		game_load = CC_GAME_LOAD_NONE;
		return;
	}

	get_monotonic_boottime(&now);

	/* assume high loading in start */
	if (game_load == CC_GAME_LOAD_NONE) {
		start = now;
		game_load = CC_GAME_LOAD_HEAVY;
		goto out_vote;
	}

	mtk_get_gpu_loading(&load);
	if (load < game->light_load) {
		/* already in low. ignore */
		if (game_load == CC_GAME_LOAD_LIGHT)
			goto out_reschedule;

		/* not enough time passed to judge */
		diff = timespec_sub(now, start);
		if (diff.tv_sec <= game->light_sec)
			goto out_reschedule;

		game_load = CC_GAME_LOAD_LIGHT;
		fcc = game->light_fcc;
		icl = game->light_icl;
		goto out_vote;
	}

	/* mark current time as start */
	start = now;
	if (game_load != CC_GAME_LOAD_HEAVY)
		game_load = CC_GAME_LOAD_HEAVY;

out_vote:
	if (chgctrl_get_battery_capacity(chip) < game->lowbatt_soc) {
		fcc = game->lowbatt_fcc;
		icl = game->lowbatt_icl;
	}

	chgctrl_vote(&chip->fcc, FCC_VOTER_GAME, fcc);
	chgctrl_vote(&chip->icl, ICL_VOTER_GAME, icl);

out_reschedule:
	schedule_delayed_work(dwork, msecs_to_jiffies(10000));
}

static int chgctrl_game_init(struct chgctrl *chip)
{
	struct chgctrl_game *game = &chip->game;

	if (game->light_icl < 0)
		game->light_icl = game->icl;
	if (game->light_fcc < 0)
		game->light_fcc = game->fcc;

	if (game->lowbatt_icl < 0)
		game->lowbatt_icl = game->icl;
	if (game->lowbatt_fcc < 0)
		game->lowbatt_fcc = game->fcc;

	INIT_DELAYED_WORK(&game->dwork, chgctrl_game_work);

	if (game->icl < 0 && game->fcc < 0)
		return -EINVAL;

	game->enabled = true;

	return 0;
}

static int chgctrl_game_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	struct chgctrl_game *game = &chip->game;

	of_property_read_u32(np, "game-icl", &game->icl);
	of_property_read_u32(np, "game-fcc", &game->fcc);

	of_property_read_u32(np, "game-light-icl", &game->light_icl);
	of_property_read_u32(np, "game-light-fcc", &game->light_fcc);
	of_property_read_u32(np, "game-light-load", &game->light_load);
	of_property_read_u32(np, "game-light-sec", &game->light_sec);

	of_property_read_u32(np, "game-lowbatt-icl", &game->lowbatt_icl);
	of_property_read_u32(np, "game-lowbatt-fcc", &game->lowbatt_fcc);
	of_property_read_u32(np, "game-lowbatt-soc", &game->lowbatt_soc);

	return 0;
}

static void chgctrl_game_init_default(struct chgctrl *chip)
{
	struct chgctrl_game *game = &chip->game;

	game->icl = -1;
	game->fcc = -1;

	game->light_icl = -1;
	game->light_fcc = -1;
	game->light_load = 80;
	game->light_sec = 100;

	game->lowbatt_icl = -1;
	game->lowbatt_fcc = -1;
	game->lowbatt_soc = 15;
}

struct chgctrl_feature chgctrl_feature_game = {
	.name = "game",
	.init_default = chgctrl_game_init_default,
	.parse_dt = chgctrl_game_parse_dt,
	.init = chgctrl_game_init,
};
