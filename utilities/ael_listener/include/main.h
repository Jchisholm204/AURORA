/**
 * @file main.h
 * @author Jacob Chisholm (https://Jchisholm204.github.io)
 * @author Claude Opus 5.5 (claude-opus-5-5) Claude Code
 * @brief AEL aggregate listener entry point
 * @version 0.2
 * @date Created: 2026-11-03
 * @modified Last Modified: 2026-11-06
 *
 * @copyright Copyright (c) 2026
 */

#ifndef _MAIN_H_
#define _MAIN_H_

/* stdout logging level; ordered so each level includes the group output of
 * the levels below it. Does not affect the CSV. */
enum listener_level_e {
    /* Internal listener events only */
    eLISTENER_LEVEL_NONE,
    /* Group connect/disconnect */
    eLISTENER_LEVEL_GROUP_LIGHT,
    /* All group events */
    eLISTENER_LEVEL_GROUP_ALL,
    /* group-all, plus per-rank connect/disconnect/failure */
    eLISTENER_LEVEL_LIGHT,
    /* group-all, plus every per-rank event */
    eLISTENER_LEVEL_ALL,
};

typedef enum listener_level_e eLISTENER_level;

int main(int, char **);

#endif
