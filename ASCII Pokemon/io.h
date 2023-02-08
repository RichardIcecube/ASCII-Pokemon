#ifndef IO_H
# define IO_H

typedef struct character character_t;
typedef int16_t pair_t[2];

void io_init_terminal(void);
void io_reset_terminal(void);
void io_display(void);
void io_handle_input(pair_t dest);
void io_queue_message(const char *format, ...);
void io_battle(character_t *aggressor, character_t *defender);
void io_trainer_battle();
void io_choose_starter(void);
void io_encounter_pokemon(void);
int io_forget_a_move(const char *species, const char *move1, const char *move2, const char *move3, const char *move4, const char *move5);

#endif
