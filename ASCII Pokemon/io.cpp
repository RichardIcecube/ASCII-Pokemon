#include <unistd.h>
#include <ncurses.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <cstring>

#include "io.h"
#include "poke327.h"
#include "pokemon.h"

typedef struct io_message {
  /* Will print " --more-- " at end of line when another message follows. *
   * Leave 10 extra spaces for that.                                      */
  char msg[71];
  struct io_message *next;
} io_message_t;

static io_message_t *io_head, *io_tail;

void io_init_terminal(void)
{
  initscr();
  raw();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  start_color();
  init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
}

void io_reset_terminal(void)
{
  endwin();

  while (io_head) {
    io_tail = io_head;
    io_head = io_head->next;
    free(io_tail);
  }
  io_tail = NULL;
}

void io_queue_message(const char *format, ...)
{
  io_message_t *tmp;
  va_list ap;

  if (!(tmp = (io_message_t *) malloc(sizeof (*tmp)))) {
    perror("malloc");
    exit(1);
  }

  tmp->next = NULL;

  va_start(ap, format);

  vsnprintf(tmp->msg, sizeof (tmp->msg), format, ap);

  va_end(ap);

  if (!io_head) {
    io_head = io_tail = tmp;
  } else {
    io_tail->next = tmp;
    io_tail = tmp;
  }
}

static void io_print_message_queue(uint32_t y, uint32_t x)
{
  while (io_head) {
    io_tail = io_head;
    attron(COLOR_PAIR(COLOR_CYAN));
    mvprintw(y, x, "%-80s", io_head->msg);
    attroff(COLOR_PAIR(COLOR_CYAN));
    io_head = io_head->next;
    if (io_head) {
      attron(COLOR_PAIR(COLOR_CYAN));
      mvprintw(y, x + 70, "%10s", " --more-- ");
      attroff(COLOR_PAIR(COLOR_CYAN));
      refresh();
      getch();
    }
    free(io_tail);
  }
  io_tail = NULL;
}

/**************************************************************************
 * Compares trainer distances from the PC according to the rival distance *
 * map.  This gives the approximate distance that the PC must travel to   *
 * get to the trainer (doesn't account for crossing buildings).  This is  *
 * not the distance from the NPC to the PC unless the NPC is a rival.     *
 *                                                                        *
 * Not a bug.                                                             *
 **************************************************************************/
static int compare_trainer_distance(const void *v1, const void *v2)
{
  const character *const *c1 = (const character * const *) v1;
  const character *const *c2 = (const character * const *) v2;

  return (world.rival_dist[(*c1)->pos[dim_y]][(*c1)->pos[dim_x]] -
          world.rival_dist[(*c2)->pos[dim_y]][(*c2)->pos[dim_x]]);
}

static character *io_nearest_visible_trainer()
{
  character **c, *n;
  uint32_t x, y, count;

  c = (character **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  n = c[0];

  free(c);

  return n;
}

void io_display()
{
  uint32_t y, x;
  character *c;

  clear();
  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (world.cur_map->cmap[y][x]) {
        mvaddch(y + 1, x, world.cur_map->cmap[y][x]->symbol);
      } else {
        switch (world.cur_map->map[y][x]) {
        case ter_boulder:
        case ter_mountain:
          attron(COLOR_PAIR(COLOR_MAGENTA));
          mvaddch(y + 1, x, '%');
          attroff(COLOR_PAIR(COLOR_MAGENTA));
          break;
        case ter_tree:
        case ter_forest:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '^');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_path:
        case ter_exit:
          attron(COLOR_PAIR(COLOR_YELLOW));
          mvaddch(y + 1, x, '#');
          attroff(COLOR_PAIR(COLOR_YELLOW));
          break;
        case ter_mart:
          attron(COLOR_PAIR(COLOR_BLUE));
          mvaddch(y + 1, x, 'M');
          attroff(COLOR_PAIR(COLOR_BLUE));
          break;
        case ter_center:
          attron(COLOR_PAIR(COLOR_RED));
          mvaddch(y + 1, x, 'C');
          attroff(COLOR_PAIR(COLOR_RED));
          break;
        case ter_grass:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, ':');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        case ter_clearing:
          attron(COLOR_PAIR(COLOR_GREEN));
          mvaddch(y + 1, x, '.');
          attroff(COLOR_PAIR(COLOR_GREEN));
          break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          attron(COLOR_PAIR(COLOR_CYAN));
          mvaddch(y + 1, x, '0');
          attroff(COLOR_PAIR(COLOR_CYAN)); 
       }
      }
    }
  }

  mvprintw(23, 1, "PC position is (%2d,%2d) on map %d%cx%d%c.",
           world.pc.pos[dim_x],
           world.pc.pos[dim_y],
           abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_x] - (WORLD_SIZE / 2) >= 0 ? 'E' : 'W',
           abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)),
           world.cur_idx[dim_y] - (WORLD_SIZE / 2) <= 0 ? 'N' : 'S');
  mvprintw(22, 1, "%d known %s.", world.cur_map->num_trainers,
           world.cur_map->num_trainers > 1 ? "trainers" : "trainer");
  mvprintw(22, 30, "Nearest visible trainer: ");
  if ((c = io_nearest_visible_trainer())) {
    attron(COLOR_PAIR(COLOR_RED));
    mvprintw(22, 55, "%c at %d %c by %d %c.",
             c->symbol,
             abs(c->pos[dim_y] - world.pc.pos[dim_y]),
             ((c->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              'N' : 'S'),
             abs(c->pos[dim_x] - world.pc.pos[dim_x]),
             ((c->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              'W' : 'E'));
    attroff(COLOR_PAIR(COLOR_RED));
  } else {
    attron(COLOR_PAIR(COLOR_BLUE));
    mvprintw(22, 55, "NONE.");
    attroff(COLOR_PAIR(COLOR_BLUE));
  }

  io_print_message_queue(0, 0);

  refresh();
}

uint32_t io_teleport_pc(pair_t dest)
{
  /* Just for fun. And debugging.  Mostly debugging. */

  do {
    dest[dim_x] = rand_range(1, MAP_X - 2);
    dest[dim_y] = rand_range(1, MAP_Y - 2);
  } while (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]                  ||
           move_cost[char_pc][world.cur_map->map[dest[dim_y]]
                                                [dest[dim_x]]] == INT_MAX ||
           world.rival_dist[dest[dim_y]][dest[dim_x]] < 0);

  return 0;
}

static void io_scroll_trainer_list(char (*s)[40], uint32_t count)
{
  uint32_t offset;
  uint32_t i;

  offset = 0;

  while (1) {
    for (i = 0; i < 13; i++) {
      mvprintw(i + 6, 19, " %-40s ", s[i + offset]);
    }
    switch (getch()) {
    case KEY_UP:
      if (offset) {
        offset--;
      }
      break;
    case KEY_DOWN:
      if (offset < (count - 13)) {
        offset++;
      }
      break;
    case 27:
      return;
    }

  }
}

static void io_list_trainers_display(npc **c,
                                     uint32_t count)
{
  uint32_t i;
  char (*s)[40]; /* pointer to array of 40 char */

  s = (char (*)[40]) malloc(count * sizeof (*s));

  mvprintw(3, 19, " %-40s ", "");
  /* Borrow the first element of our array for this string: */
  snprintf(s[0], 40, "You know of %d trainers:", count);
  mvprintw(4, 19, " %-40s ", *s);
  mvprintw(5, 19, " %-40s ", "");

  for (i = 0; i < count; i++) {
    snprintf(s[i], 40, "%16s %c: %2d %s by %2d %s",
             char_type_name[c[i]->ctype],
             c[i]->symbol,
             abs(c[i]->pos[dim_y] - world.pc.pos[dim_y]),
             ((c[i]->pos[dim_y] - world.pc.pos[dim_y]) <= 0 ?
              "North" : "South"),
             abs(c[i]->pos[dim_x] - world.pc.pos[dim_x]),
             ((c[i]->pos[dim_x] - world.pc.pos[dim_x]) <= 0 ?
              "West" : "East"));
    if (count <= 13) {
      /* Handle the non-scrolling case right here. *
       * Scrolling in another function.            */
      mvprintw(i + 6, 19, " %-40s ", s[i]);
    }
  }

  if (count <= 13) {
    mvprintw(count + 6, 19, " %-40s ", "");
    mvprintw(count + 7, 19, " %-40s ", "Hit escape to continue.");
    while (getch() != 27 /* escape */)
      ;
  } else {
    mvprintw(19, 19, " %-40s ", "");
    mvprintw(20, 19, " %-40s ",
             "Arrows to scroll, escape to continue.");
    io_scroll_trainer_list(s, count);
  }

  free(s);
}

static void io_list_trainers()
{
  npc **c;
  uint32_t x, y, count;

  c = (npc **) malloc(world.cur_map->num_trainers * sizeof (*c));

  /* Get a linear list of trainers */
  for (count = 0, y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (world.cur_map->cmap[y][x] && world.cur_map->cmap[y][x] !=
          &world.pc) {
        c[count++] = (npc *) world.cur_map->cmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  qsort(c, count, sizeof (*c), compare_trainer_distance);

  /* Display it */
  io_list_trainers_display(c, count);
  free(c);

  /* And redraw the map */
  io_display();
}

void io_top_clear()
{
  mvprintw(0, 0, "                                                                               .");
}

void io_top2_clear()
{
  mvprintw(1, 0, "                                                                               .");
}

void io_buy()
{
  bool loop = true;
  int key;
  io_top_clear();
  mvprintw(0, 0, "What do you want to buy? Wallet: $%d" , world.pc.wallet);
  while(loop)
  {
    io_top2_clear();
    mvprintw(1, 0, "1. Potion - $50 2. Revive - $200 3. Pokeball - $100 4. Exit");
    switch(key = getch())
    {
      case '1':
        if(world.pc.wallet < 50)
        {
          io_top_clear();
          mvprintw(0, 0, "You can't afford that!");
          getch();
        }
        else
        {
          io_top_clear();
          world.pc.potions++;
          mvprintw(0, 0, "Thank you for your purchase! You now have %d potions.", world.pc.potions);
          world.pc.wallet -= 50;
          getch();
        }
        break;
      case '2':
        if(world.pc.wallet < 200)
        {
          io_top_clear();
          mvprintw(0, 0, "You can't afford that!");
          getch();
        }
        else
        {
          io_top_clear();
          world.pc.revives++;
          mvprintw(0, 0, "Thank you for your purchase! You now have %d revives.", world.pc.revives);
          world.pc.wallet -= 200;
          getch();
        }
        break;
      case '3':
        if(world.pc.wallet < 100)
        {
          io_top_clear();
          mvprintw(0, 0, "You can't afford that!");
          getch();
        }
        else
        {
          io_top_clear();
          world.pc.pokeballs++;
          mvprintw(0, 0, "Thank you for your purchase! You now have %d Pokeballs.", world.pc.pokeballs);
          world.pc.wallet -= 100;
          getch();
        }
        break;
      case '4':
        return;
        break;
      default:
        break;
    }
    io_top_clear();
    mvprintw(0, 0, "Anything else? Wallet: $%d", world.pc.wallet);
  }
}

void io_sell()
{
  bool loop = true;
  int key;
  io_top_clear();
  mvprintw(0, 0, "What do you want to sell? Wallet: %d", world.pc.wallet);
  while(loop)
  {
    io_top2_clear();
    mvprintw(1, 0, "1. Potions x %d 2. Revives x %d 3. Pokeballs x %d 4. Exit", world.pc.potions, world.pc.revives, world.pc.pokeballs);
    switch(key = getch())
    {
      case '1':
        if(world.pc.potions == 0)
        {
          io_top_clear();
          mvprintw(0, 0, "You don't have any potions to sell!");
          getch();
        }
        else
        {
          io_top_clear();
          world.pc.potions--;
          mvprintw(0, 0, "Thank you for the potion! Here is $25. You now have %d potions left.", world.pc.potions);
          world.pc.wallet += 25;
          getch();
        }
        break;
      case '2':
        if(world.pc.revives == 0)
        {
          io_top_clear();
          mvprintw(0, 0, "You don't have any revives to sell!");
          getch();
        }
        else
        {
          io_top_clear();
          world.pc.revives--;
          mvprintw(0, 0, "Thank you for the revive! Here is $100. You now have %d revives left.", world.pc.revives);
          world.pc.wallet += 100;
          getch();
        }
        break;
      case '3':
        if(world.pc.pokeballs == 0)
        {
          io_top_clear();
          mvprintw(0, 0, "You don't have any Pokeballs to sell!");
          getch();
        }
        else
        {
          io_top_clear();
          world.pc.pokeballs--;
          mvprintw(0, 0, "Thank you for the Pokeball! Here is $50. You now have %d Pokeballs left.", world.pc.pokeballs);
          world.pc.wallet += 50;
          getch();
        }
        break;
      case '4':
        return;
        break;
      default:
        break;
    }
    io_top_clear();
    mvprintw(0, 0, "Anything else? Wallet: $%d", world.pc.wallet);
  }
}

void io_pokemart()
{
  int key;
  bool loop = true;
  while(loop)
  {  
    io_top_clear();
    mvprintw(0, 0, "Welcome to the Pokemart. How can I help you?");
    io_top2_clear();
    mvprintw(1, 0, "1. Buy 2. Sell 3. Leave");
    switch(key = getch())
    {
      case '1':
        io_buy();
        break;
      case '2':
        io_sell();
        break;
      case '3':
        loop = false;
        break;  
      default:
        break;
    }
  }
  refresh();
  getch();
}

void io_display_current_party()
{
  io_top2_clear();
  if(world.pc.party[1] == NULL)
  {
    mvprintw(1, 0, "1. %s %d/%d", world.pc.party[0]->get_species(), world.pc.current_party_hp[0], world.pc.party[0]->get_hp());
  }
  else if(world.pc.party[2] == NULL)
  {
    mvprintw(1, 0, "1. %s %d/%d 2. %s %d/%d", world.pc.party[0]->get_species(), world.pc.current_party_hp[0], world.pc.party[0]->get_hp(), world.pc.party[1]->get_species(), world.pc.current_party_hp[1], world.pc.party[1]->get_hp());
  }
  else if(world.pc.party[3] == NULL)
  {
    mvprintw(1, 0, "1. %s %d/%d 2. %s %d/%d 3. %s %d/%d", world.pc.party[0]->get_species(), world.pc.current_party_hp[0], world.pc.party[0]->get_hp(), world.pc.party[1]->get_species(), world.pc.current_party_hp[1], world.pc.party[1]->get_hp(), world.pc.party[2]->get_species(), world.pc.current_party_hp[2], world.pc.party[2]->get_hp());
  }
  else if(world.pc.party[4] == NULL)
  {
    mvprintw(1, 0, "1. %s %d/%d 2. %s %d/%d 3. %s %d/%d 4. %s %d/%d", world.pc.party[0]->get_species(), world.pc.current_party_hp[0], world.pc.party[0]->get_hp(), world.pc.party[1]->get_species(), world.pc.current_party_hp[1], world.pc.party[1]->get_hp(), world.pc.party[2]->get_species(), world.pc.current_party_hp[2], world.pc.party[2]->get_hp(), world.pc.party[3]->get_species(), world.pc.current_party_hp[3], world.pc.party[3]->get_hp());
  }
  else if(world.pc.party[5] == NULL)
  {
    mvprintw(1, 0, "1. %s %d/%d 2. %s %d/%d 3. %s %d/%d 4. %s %d/%d 5. %s %d/%d", world.pc.party[0]->get_species(), world.pc.current_party_hp[0], world.pc.party[0]->get_hp(), world.pc.party[1]->get_species(), world.pc.current_party_hp[1], world.pc.party[1]->get_hp(), world.pc.party[2]->get_species(), world.pc.current_party_hp[2], world.pc.party[2]->get_hp(), world.pc.party[3]->get_species(), world.pc.current_party_hp[3], world.pc.party[3]->get_hp(), world.pc.party[4]->get_species(), world.pc.current_party_hp[4], world.pc.party[4]->get_hp());
  }
  else 
  {
    mvprintw(1, 0, "1. %s %d/%d 2. %s %d/%d 3. %s %d/%d 4. %s %d/%d 5. %s %d/%d 6. %s %d/%d", world.pc.party[0]->get_species(), world.pc.current_party_hp[0], world.pc.party[0]->get_hp(), world.pc.party[1]->get_species(), world.pc.current_party_hp[1], world.pc.party[1]->get_hp(), world.pc.party[2]->get_species(), world.pc.current_party_hp[2], world.pc.party[2]->get_hp(), world.pc.party[3]->get_species(), world.pc.current_party_hp[3], world.pc.party[3]->get_hp(), world.pc.party[4]->get_species(), world.pc.current_party_hp[4], world.pc.party[4]->get_hp(), world.pc.party[5]->get_species(), world.pc.current_party_hp[5], world.pc.party[5]->get_hp());
  }
}

void io_deposit_clean_up(int p)
{
  int party_size = 0;  
  for(int j = 0; j < 6; j++)
  {  
    if(world.pc.current_party_hp[j] != -1)
    {
      party_size++;
    }
  }
  for(int i = p; i < party_size; i++)
  {
    world.pc.party[i] = (pokemon *) malloc(sizeof(*world.pc.party[i]));
    memcpy(world.pc.party[i], world.pc.party[i + 1], sizeof(pokemon));
    world.pc.current_party_hp[i] = world.pc.current_party_hp[i + 1];
    world.pc.current_party_hp[i + 1] = -1;
    free(world.pc.party[i + 1]);
    world.pc.party[i + 1] = NULL;
  }
}

void io_deposit()
{
  bool loop = true;
  int key;
  int first_empty_box_slot = 0;
  while(world.pc.box[first_empty_box_slot] != NULL)
  {
    first_empty_box_slot++;
  }
  while(loop)
  {
    io_top_clear();
    mvprintw(0, 0, "Select a Pokemon to deposit or press 'q' to exit.");
    io_display_current_party();
    switch(key = getch())
    {
      case '1':
        if(world.pc.current_party_hp[1] == -1)
        {
          io_top_clear();
          mvprintw(0, 0, "You must have at least 1 Pokemon in your party!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Depositing %s%s%s...", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
          getch();
          world.pc.box[first_empty_box_slot] = (pokemon *) malloc(sizeof(*world.pc.box[first_empty_box_slot]));
          memcpy(world.pc.box[first_empty_box_slot], world.pc.party[0], sizeof(pokemon));
          first_empty_box_slot++;
          free(world.pc.party[0]);
          world.pc.party[0] = NULL;
          world.pc.current_party_hp[0] = -1;
          io_deposit_clean_up(0);
          io_top_clear();
          mvprintw(0, 0, "Successfully deposited!");
          getch();
        }
        break;
      case '2':
        if(world.pc.current_party_hp[1] == -1)
        {
          io_top_clear();
          mvprintw(0, 0, "You don't have a Pokemon in that slot!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Depositing %s%s%s...", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
          getch();
          world.pc.box[first_empty_box_slot] = (pokemon *) malloc(sizeof(*world.pc.box[first_empty_box_slot]));
          memcpy(world.pc.box[first_empty_box_slot], world.pc.party[1], sizeof(pokemon));
          first_empty_box_slot++;
          free(world.pc.party[1]);
          world.pc.party[1] = NULL;
          world.pc.current_party_hp[1] = -1;
          io_deposit_clean_up(1);
          io_top_clear();
          mvprintw(0, 0, "Successfully deposited!");
          getch();
        }
        break;
      case '3':
        if(world.pc.current_party_hp[2] == -1)
        {
          io_top_clear();
          mvprintw(0, 0, "You don't have a Pokemon in that slot!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Depositing %s%s%s...", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
          getch();
          world.pc.box[first_empty_box_slot] = (pokemon *) malloc(sizeof(*world.pc.box[first_empty_box_slot]));
          memcpy(world.pc.box[first_empty_box_slot], world.pc.party[2], sizeof(pokemon));
          first_empty_box_slot++;
          free(world.pc.party[2]);
          world.pc.party[2] = NULL;
          world.pc.current_party_hp[2] = -1;
          io_deposit_clean_up(2);
          io_top_clear();
          mvprintw(0, 0, "Successfully deposited!");
          getch();
        }
        break;
      case '4':
        if(world.pc.current_party_hp[3] == -1)
        {
          io_top_clear();
          mvprintw(0, 0, "You don't have a Pokemon in that slot!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Depositing %s%s%s...", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
          getch();
          world.pc.box[first_empty_box_slot] = (pokemon *) malloc(sizeof(*world.pc.box[first_empty_box_slot]));
          memcpy(world.pc.box[first_empty_box_slot], world.pc.party[3], sizeof(pokemon));
          first_empty_box_slot++;
          free(world.pc.party[3]);
          world.pc.party[3] = NULL;
          world.pc.current_party_hp[3] = -1;
          io_deposit_clean_up(3);
          io_top_clear();
          mvprintw(0, 0, "Successfully deposited!");
          getch();
        }
        break;
      case '5':
        if(world.pc.current_party_hp[4] == -1)
        {
          io_top_clear();
          mvprintw(0, 0, "You don't have a Pokemon in that slot!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Depositing %s%s%s...", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
          getch();
          world.pc.box[first_empty_box_slot] = (pokemon *) malloc(sizeof(*world.pc.box[first_empty_box_slot]));
          memcpy(world.pc.box[first_empty_box_slot], world.pc.party[4], sizeof(pokemon));
          first_empty_box_slot++;
          free(world.pc.party[4]);
          world.pc.party[4] = NULL;
          world.pc.current_party_hp[4] = -1;
          io_deposit_clean_up(4);
          io_top_clear();
          mvprintw(0, 0, "Successfully deposited!");
          getch();
        }
        break;
      case '6':
        if(world.pc.current_party_hp[5] == -1)
        {
          io_top_clear();
          mvprintw(0, 0, "You don't have a Pokemon in that slot!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Depositing %s%s%s...", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
          getch();
          world.pc.box[first_empty_box_slot] = (pokemon *) malloc(sizeof(*world.pc.box[first_empty_box_slot]));
          memcpy(world.pc.box[first_empty_box_slot], world.pc.party[5], sizeof(pokemon));
          first_empty_box_slot++;
          free(world.pc.party[5]);
          world.pc.party[5] = NULL;
          world.pc.current_party_hp[5] = -1;
          io_top_clear();
          mvprintw(0, 0, "Successfully deposited!");
          getch();
        }
        break;
      case 'q':
        loop = false;
        break;
    }
  }
}

void io_withdraw_clean_up(int index)
{
  while(world.pc.box[index + 1] != NULL && index < 99)
  {
    world.pc.box[index] = (pokemon *) malloc(sizeof(*world.pc.box[index]));
    memcpy(world.pc.box[index], world.pc.box[index + 1], sizeof(pokemon));
    free(world.pc.box[index + 1]);
    world.pc.box[index + 1] = NULL;
    index++;
  } 
}

void io_withdraw()
{
  int key;
  bool loop = true;
  int first_open_party_slot = 0;
  int page = 0;
  while(world.pc.current_party_hp[first_open_party_slot] != -1)
  {
    first_open_party_slot++;
  }
  if(world.pc.box[0] == NULL)
  {
    io_top_clear();
    mvprintw(0, 0, "Box is empty!");
    getch();
    return;
  }
  while(loop)
  {
    io_top_clear();
    mvprintw(0, 0, "Select a Pokemon to withdraw or press 'q' to quit.");
    if(world.pc.box[page + 1] == NULL)
    {
      io_top2_clear();
      mvprintw(1, 0, "1. %s%s%s", world.pc.box[page]->is_shiny() ? "*" : "", world.pc.box[page]->get_species(), world.pc.box[page]->is_shiny() ? "*" : "");
    }
    else if(world.pc.box[page + 2] == NULL)
    {
      io_top2_clear();
      mvprintw(1, 0, "1. %s%s%s 2. %s%s%s", world.pc.box[page]->is_shiny() ? "*" : "", world.pc.box[page]->get_species(), world.pc.box[page]->is_shiny() ? "*" : "", world.pc.box[page + 1]->is_shiny() ? "*" : "", world.pc.box[page + 1]->get_species(), world.pc.box[page + 1]->is_shiny() ? "*" : "");
    }
    else if(world.pc.box[page + 3] == NULL)
    {
      io_top2_clear();
      mvprintw(1, 0, "1. %s%s%s 2. %s%s%s 3. %s%s%s", world.pc.box[page]->is_shiny() ? "*" : "", world.pc.box[page]->get_species(), world.pc.box[page]->is_shiny() ? "*" : "", world.pc.box[page + 1]->is_shiny() ? "*" : "", world.pc.box[page + 1]->get_species(), world.pc.box[page + 1]->is_shiny() ? "*" : "", world.pc.box[page + 2]->is_shiny() ? "*" : "", world.pc.box[page + 2]->get_species(), world.pc.box[page + 2]->is_shiny() ? "*" : "");
    }
    else if(world.pc.box[page + 4] == NULL)
    {
      io_top2_clear();
      mvprintw(1, 0, "1. %s%s%s 2. %s%s%s 3. %s%s%s 4. %s%s%s", world.pc.box[page]->is_shiny() ? "*" : "", world.pc.box[page]->get_species(), world.pc.box[page]->is_shiny() ? "*" : "", world.pc.box[page + 1]->is_shiny() ? "*" : "", world.pc.box[page + 1]->get_species(), world.pc.box[page + 1]->is_shiny() ? "*" : "", world.pc.box[page + 2]->is_shiny() ? "*" : "", world.pc.box[page + 2]->get_species(), world.pc.box[page + 2]->is_shiny() ? "*" : "", world.pc.box[page + 3]->is_shiny() ? "*" : "", world.pc.box[page + 3]->get_species(), world.pc.box[page + 3]->is_shiny() ? "*" : "");
    }
    else
    {
      io_top2_clear();
      mvprintw(1, 0, "1. %s%s%s 2. %s%s%s 3. %s%s%s 4. %s%s%s 5. %s%s%s %s", world.pc.box[page]->is_shiny() ? "*" : "", world.pc.box[page]->get_species(), world.pc.box[page]->is_shiny() ? "*" : "", world.pc.box[page + 1]->is_shiny() ? "*" : "", world.pc.box[page + 1]->get_species(), world.pc.box[page + 1]->is_shiny() ? "*" : "", world.pc.box[page + 2]->is_shiny() ? "*" : "", world.pc.box[page + 2]->get_species(), world.pc.box[page + 2]->is_shiny() ? "*" : "", world.pc.box[page + 3]->is_shiny() ? "*" : "", world.pc.box[page + 3]->get_species(), world.pc.box[page + 3]->is_shiny() ? "*" : "", world.pc.box[page + 4]->is_shiny() ? "*" : "", world.pc.box[page + 4]->get_species(), world.pc.box[page + 4]->is_shiny() ? "*" : "", (world.pc.box[page + 5] == NULL) ? "" : "6. Next");
    }
    switch(key = getch())
    {
      case '1': //can never be empty
        if(world.pc.box[page * 5] == NULL)
        {
          io_top_clear();
          mvprintw(0, 0, "No Pokemon in that slot! Choose a slot with a Pokemon in it!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Withdrawing %s%s%s...", world.pc.box[page * 5]->is_shiny() ? "*" : "", world.pc.box[page * 5]->get_species(), world.pc.box[page * 5]->is_shiny() ? "*" : "");
          getch();
          world.pc.party[first_open_party_slot] = (pokemon *) malloc(sizeof(*world.pc.party[first_open_party_slot]));
          memcpy(world.pc.party[first_open_party_slot], world.pc.box[page * 5], sizeof(pokemon));
          world.pc.current_party_hp[first_open_party_slot] = world.pc.party[first_open_party_slot]->get_hp();
          free(world.pc.box[page * 5]);
          world.pc.box[page * 5] = NULL;
          io_withdraw_clean_up(page * 5);
          io_top_clear();
          mvprintw(0, 0, "Withdraw complete.");
          getch();
          return;
        }  
        break;
      case '2':
        if(world.pc.box[(page * 5) + 1] == NULL)
        {
          io_top_clear();
          mvprintw(0, 0, "No Pokemon in that slot! Choose a slot with a Pokemon in it!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Withdrawing %s%s%s...", world.pc.box[(page * 5) + 1]->is_shiny() ? "*" : "", world.pc.box[(page * 5) + 1]->get_species(), world.pc.box[(page * 5) + 1]->is_shiny() ? "*" : "");
          getch();
          world.pc.party[first_open_party_slot] = (pokemon *) malloc(sizeof(*world.pc.party[first_open_party_slot]));
          memcpy(world.pc.party[first_open_party_slot], world.pc.box[(page * 5) + 1], sizeof(pokemon));
          world.pc.current_party_hp[first_open_party_slot] = world.pc.party[first_open_party_slot]->get_hp();
          free(world.pc.box[(page * 5) + 1]);
          world.pc.box[(page * 5) + 1] = NULL;
          io_withdraw_clean_up((page * 5) + 1);
          io_top_clear();
          mvprintw(0, 0, "Withdraw complete.");
          getch();
          return;
        }
        break;
      case '3':
        if(world.pc.box[(page * 5) + 2] == NULL)
        {
          io_top_clear();
          mvprintw(0, 0, "No Pokemon in that slot! Choose a slot with a Pokemon in it!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Withdrawing %s%s%s...", world.pc.box[(page * 5) + 2]->is_shiny() ? "*" : "", world.pc.box[(page * 5) + 2]->get_species(), world.pc.box[(page * 5) + 2]->is_shiny() ? "*" : "");
          getch();
          world.pc.party[first_open_party_slot] = (pokemon *) malloc(sizeof(*world.pc.party[first_open_party_slot]));
          memcpy(world.pc.party[first_open_party_slot], world.pc.box[(page * 5) + 2], sizeof(pokemon));
          world.pc.current_party_hp[first_open_party_slot] = world.pc.party[first_open_party_slot]->get_hp();
          free(world.pc.box[(page * 5) + 2]);
          world.pc.box[(page * 5) + 2] = NULL;
          io_withdraw_clean_up((page * 5) + 2);
          io_top_clear();
          mvprintw(0, 0, "Withdraw complete.");
          getch();
          return;
        }
        break;
      case '4':
        if(world.pc.box[(page * 5) + 3] == NULL)
        {
          io_top_clear();
          mvprintw(0, 0, "No Pokemon in that slot! Choose a slot with a Pokemon in it!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Withdrawing %s%s%s...", world.pc.box[(page * 5) + 3]->is_shiny() ? "*" : "", world.pc.box[(page * 5) + 3]->get_species(), world.pc.box[(page * 5) + 3]->is_shiny() ? "*" : "");
          getch();
          world.pc.party[first_open_party_slot] = (pokemon *) malloc(sizeof(*world.pc.party[first_open_party_slot]));
          memcpy(world.pc.party[first_open_party_slot], world.pc.box[(page * 5) + 3], sizeof(pokemon));
          world.pc.current_party_hp[first_open_party_slot] = world.pc.party[first_open_party_slot]->get_hp();
          free(world.pc.box[(page * 5) + 3]);
          world.pc.box[(page * 5) + 3] = NULL;
          io_withdraw_clean_up((page * 5) + 3);
          io_top_clear();
          mvprintw(0, 0, "Withdraw complete.");
          getch();
          return;
        }
        break;
      case '5':
        if(world.pc.box[(page * 5) + 4] == NULL)
        {
          io_top_clear();
          mvprintw(0, 0, "No Pokemon in that slot! Choose a slot with a Pokemon in it!");
          getch();
        }
        else
        {
          io_top_clear();
          mvprintw(0, 0, "Withdrawing %s%s%s...", world.pc.box[(page * 5) + 4]->is_shiny() ? "*" : "", world.pc.box[(page * 5) + 4]->get_species(), world.pc.box[(page * 5) + 4]->is_shiny() ? "*" : "");
          getch();
          world.pc.party[first_open_party_slot] = (pokemon *) malloc(sizeof(*world.pc.party[first_open_party_slot]));
          memcpy(world.pc.party[first_open_party_slot], world.pc.box[(page * 5) + 4], sizeof(pokemon));
          world.pc.current_party_hp[first_open_party_slot] = world.pc.party[first_open_party_slot]->get_hp();
          free(world.pc.box[(page * 5) + 4]);
          world.pc.box[(page * 5) + 4] = NULL;
          io_withdraw_clean_up((page * 5) + 4);
          io_top_clear();
          mvprintw(0, 0, "Withdraw complete.");
          getch();
          return;
        }
        break;
      case '6':
        if(world.pc.box[(page + 1) * 5] == NULL)
        {
          io_top_clear();
          mvprintw(0, 0, "Invalid input! No more pages.");
          getch();
        }
        else
        {
          page++;
        }
        break;
      case 'q':
        return;
        break;
    }
    if(world.pc.current_party_hp[5] != -1) //if party is full, exit
    {
      return;
    }
  }
}

bool io_display_pc()
{
  bool loop = true;
  int key;
  while(loop)
  {
    io_top_clear();
    mvprintw(0, 0, "What do you wish to do?");
    io_top2_clear();
    mvprintw(1, 0, "1. Deposit 2. Withdraw 3. Exit");
    switch(key = getch())
    {
      case '1':
        if(world.pc.current_party_hp[1] == -1)
        {
          io_top_clear();
          mvprintw(0, 0, "You must have at least 1 Pokemon in your party!");
          getch();
        }
        else
        {
          io_deposit();
        }
        break;
      case '2':
        if(world.pc.current_party_hp[5] != -1)
        {
          io_top_clear();
          mvprintw(0, 0, "Your party is full! Please deposit a Pokemon before trying to withdraw!");
          getch();
        }
        else
        {
          io_withdraw();
        }
        break;
      case '3':
        loop = false;
        break;
      default:
        break;
    }
  }
  return true;
}

void io_pokemon_center()
{
  int key;
  bool loop = true;
  io_top_clear();
  mvprintw(0, 0, "Welcome to the Pokemon Center.  Your Pokemon are all fully healed!");
  for(int i = 0; i < 6; i++)
  {
    if(world.pc.current_party_hp[i] != -1)
    {
      world.pc.current_party_hp[i] = world.pc.party[i]->get_hp();
    }
  }
  refresh();
  getch();
  
  while(loop)
  {
    io_top_clear();
    mvprintw(0,0, "Do you want to open your PC?");
    io_top2_clear();
    mvprintw(1, 0, "1: Open PC 2: Leave Pokemon Center");
    switch(key = getch())
    {
      case '1':
        loop = io_display_pc();
        break;
      case '2':
        loop = false;
        break;
      default:
        break; 
    }
  }
}

bool io_use_potion(int slot)
{
  io_top_clear();
  if(world.pc.party[slot] == NULL)
  {
    mvprintw(0, 0, "No Pokemon in that slot!");
    getch();
  }
  else if(world.pc.party[slot]->get_hp() == world.pc.current_party_hp[slot])
  {
    mvprintw(0, 0, "%s%s%s is already at max hp!", world.pc.party[slot]->is_shiny() ? "*" : "", world.pc.party[slot]->get_species(), world.pc.party[slot]->is_shiny() ? "*" : "");
    getch();
  }
  else if(world.pc.current_party_hp[slot] == 0)
  {
    mvprintw(0, 0, "%s%s%s is unconscious! Use a revive first!", world.pc.party[slot]->is_shiny() ? "*" : "", world.pc.party[slot]->get_species(), world.pc.party[slot]->is_shiny() ? "*" : "");
    getch();
  }
  else
  {
    world.pc.current_party_hp[slot] += 20;
    if(world.pc.current_party_hp[slot] > world.pc.party[slot]->get_hp()) world.pc.current_party_hp[slot] = world.pc.party[slot]->get_hp();
    mvprintw(0, 0, "Restored 20 hp to %s%s%s!", world.pc.party[slot]->is_shiny() ? "*" : "", world.pc.party[slot]->get_species(), world.pc.party[slot]->is_shiny() ? "*" : "");
    world.pc.potions--;
    getch();
    return false;
  }
  return true;
} 
 
bool io_potion()
{
  int key;
  bool loop2 = true;
  int num_damaged = 0;
  if(world.pc.potions == 0)
  {
    io_top2_clear();
    mvprintw(1, 0, "Out of potions!");
    getch();
  }
  else
  {
    for(int i = 0; i < 6; i++)
    {
    
      if(world.pc.current_party_hp[i] != -1 && world.pc.current_party_hp[i] != world.pc.party[i]->get_hp()) num_damaged++;
    }
    if(num_damaged == 0)
    {
      io_top_clear();
      mvprintw(0, 0, "No Pokemon to heal!");
      getch();
    }
    else
    {
      while(loop2)
      {
        mvprintw(0, 0, "Choose a Pokemon to use a potion on or press 'q' to use a different item.");
        io_display_current_party();
        switch(key = getch())
        {
          case '1':
            loop2 = io_use_potion(0);
            break;
          case '2':
            loop2 = io_use_potion(1);
            break;
          case '3':
            loop2 = io_use_potion(2);
            break;  
          case '4':
            loop2 = io_use_potion(3);
            break;
          case '5':
            loop2 = io_use_potion(4);
            break;
          case '6':
            loop2 = io_use_potion(5);
            break;      
          case 'q':
            return true;
            break;
          default:
            break;
        }
      }
      return loop2;
    }
  }
  return true;
}

bool io_use_revive(int slot)
{
  io_top_clear();
  if(world.pc.party[slot] == NULL)
  {
    mvprintw(0, 0, "No Pokemon in that slot!");
    getch();
  }
  else if(world.pc.current_party_hp[slot] -= 0)
  {
    mvprintw(0, 0, "%s%s%s hasn't fainted yet!", world.pc.party[slot]->is_shiny() ? "*" : "", world.pc.party[slot]->get_species(), world.pc.party[slot]->is_shiny() ? "*" : "");
    getch();
  }
  else
  {
    world.pc.current_party_hp[slot] = world.pc.party[slot]->get_hp() / 2;
    mvprintw(0, 0, "Revived %s%s%s!", world.pc.party[slot]->is_shiny() ? "*" : "", world.pc.party[slot]->get_species(), world.pc.party[slot]->is_shiny() ? "*" : "");
    world.pc.revives--;
    getch();
    return false;
  }
  return true;
} 

bool io_revive()
{
  int key;
  bool loop2 = true;
  int num_fainted = 0;
  io_top_clear();
  if(world.pc.revives == 0)
  {
    mvprintw(1, 0, "Out of revives!");
    getch();
  }
  else
  {
    num_fainted = 0;
    for(int i = 0; i < 6; i++)
    {
      if(world.pc.current_party_hp[i] == 0) num_fainted++;
    }    
    if(num_fainted == 0)
    {
      mvprintw(0, 0, "No Pokemon to use a revive on!");
      getch();
    }
    else
    {
      while(loop2)
      {
        mvprintw(0, 0, "Choose a Pokemon to use a revive on or press 'q' to choose a different item to use.");
        io_display_current_party();
        switch(key = getch())
        {
          case '1':
            loop2 = io_use_revive(0);
            break;
          case '2':
            loop2 = io_use_revive(1);
            break;
          case '3':
            loop2 = io_use_revive(2);
            break;  
          case '4':
            loop2 = io_use_revive(3);
            break;
          case '5':
            loop2 = io_use_revive(4);
            break;
          case '6':
            loop2 = io_use_revive(5);
            break;      
          case 'q':
            return true;
            break;
          default:
            break;
        }
        return loop2;
      }  
    }
  }
  return true;
}

bool io_catch_pokemon(pokemon *p)
{
  int first_empty_box_slot = 0;
  io_top_clear();
  if(rand() % 100 < 75)
  {
    if(world.pc.party[5] != NULL)
    {
      while(world.pc.box[first_empty_box_slot] != NULL)
      {
        first_empty_box_slot++;
      }
      world.pc.box[first_empty_box_slot] = (pokemon*) malloc(sizeof(*world.pc.box[first_empty_box_slot]));
      memcpy(world.pc.box[first_empty_box_slot], p, sizeof(pokemon));
      io_top_clear();
      mvprintw(0, 0, "%s%s%s caught!", world.pc.box[first_empty_box_slot]->is_shiny() ? "*": "", world.pc.box[first_empty_box_slot]->get_species(), world.pc.box[first_empty_box_slot]->is_shiny() ? "*" : "");
      getch();
      io_top_clear();
      mvprintw(0, 0, "No room in your party! %s%s%s sent to PC.", world.pc.box[first_empty_box_slot]->is_shiny() ? "*": "", world.pc.box[first_empty_box_slot]->get_species(), world.pc.box[first_empty_box_slot]->is_shiny() ? "*" : "");
      getch();
      return false;
    }
    else
    {
      int index = 0;
      while(world.pc.party[index] != NULL && index != 5) index++;
      world.pc.party[index] = (pokemon *) malloc(sizeof(*world.pc.party[index]));
      memcpy(world.pc.party[index], p, sizeof(pokemon));
      //world.pc.current_party_hp[index] = 0; //FOR THE PURPOSE OF TESTING REVIVE AND POTIONS
      world.pc.current_party_hp[index] = world.pc.party[index]->get_hp();
      io_top_clear();
      mvprintw(0, 0, "%s%s%s caught!", world.pc.party[index]->is_shiny() ? "*" : "",   world.pc.party[index]->get_species(), world.pc.party[index]->is_shiny() ? "*" : "");
      world.pc.pokeballs--;
      getch();
      return false;
    }
  }
  else
  { 
    io_top_clear();
    world.pc.pokeballs--;
    mvprintw(0, 0, "It got out!");
    getch();
    return true;
  }
  
}

bool io_pokeball(char type, pokemon *p)
{
  io_top_clear();
  if(type == '1')
  {
    if(world.pc.pokeballs == 0)
    {
      mvprintw(0, 0, "Out of pokeballs!");
      getch();
    }
    else 
    {
      return io_catch_pokemon(p);
    }
  }
  else if(type == '2')
  {
    mvprintw(0, 0, "You shouldn't try to steal other trainers' Pokemon!");
    getch();
  }
  else
  {
    mvprintw(0, 0, "No Pokemon to catch!");
    getch();
  }
  return true;
}

bool io_display_bag_contents(char type, pokemon *p)
{
  int key; 
  bool loop = true;
  while(loop)
  {
    io_top_clear();
    mvprintw(0, 0, "Choose an item to use.");
    io_top2_clear();
    switch(type)
    {
      case '0': // open bag in field
        mvprintw(1, 0, "Bag Contents: 1. Potions x %d 2. Revives x %d 3. Pokeballs x %d", world.pc.potions, world.pc.revives, world.pc.pokeballs);
        refresh();
        break;
      case '1': // open bag in wild battle
        mvprintw(1, 0, "Bag Contents: 1. Potions x %d 2. Revives x %d 3. Pokeballs x %d", world.pc.potions, world.pc.revives, world.pc.pokeballs);
        refresh();
        break;
      case '2': // open bag in trainer battle
        mvprintw(1, 0, "Bag Contents: 1. Potions x %d 2. Revives x %d", world.pc.potions, world.pc.revives);
        refresh();
        break;
    }
    switch(key = getch())
    {
      case '1':
        loop = io_potion();
        break;
      case '2':
        loop = io_revive();
        break;
      case '3':
        loop = io_pokeball(type, p);
        if(loop == false) return true;
        break;
      case 'q':
        return false;
        break;
      default:
        break;
    }
  }
  return false;
}

void io_choose_starter()
{
  pokemon *p1;
  pokemon *p2;
  pokemon *p3;
  bool chosen = false;
  int key;
  p1 = new pokemon(1);
  p2 = new pokemon(1);
  p3 = new pokemon(1);
  
  while(!chosen) 
  {
    mvprintw(0, 0, "Welcome to the World of Pokemon!");
    mvprintw(1, 0, "Choose a pokemon to be your starter.");
    mvprintw(2, 0, "1. %s%s%s 2. %s%s%s 3. %s%s%s", p1->is_shiny() ? "*" : "", p1->get_species(), p1->is_shiny() ? "*" : "", p2->is_shiny() ? "*" : "", p2->get_species(), p2->is_shiny() ? "*" : "", p3->is_shiny() ? "*" : "", p3->get_species(), p3->is_shiny() ? "*" : "");
    switch(key = getch())
    {
      case '1':
        world.pc.party[0] = p1;
        world.pc.current_party_hp[0] = p1->get_hp();
        chosen = true;
        break;
      case '2':
        world.pc.party[0] = p2;
        world.pc.current_party_hp[0] = p2->get_hp();
        chosen = true;
        break;
      case '3':
        world.pc.party[0] = p3;
        world.pc.current_party_hp[0] = p3->get_hp();
        chosen = true;
        break;
      default:
        break;
    }
  }
  io_queue_message("%s%s%s chosen as your starter!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
}

void io_opponent_attack(int active_pokemon, pokemon *p)
{
  int npc_damage = 0;
  int total_npc_moves = 0;
  int npc_move_index;
  double critical;
  for(int i = 0; i < 4; i++)
  {
    if(p->get_move_accuracy(i) != -1)
    {
      total_npc_moves++;
    }
  }
  npc_move_index = rand() % total_npc_moves;
  if((rand() % 100) <= 20) critical = 1.5;
  else critical = 1;
  io_top_clear();
  mvprintw(0, 0, "The wild %s%s%s used %s!", p->is_shiny() ? "*" : "", p->get_species(),p->is_shiny() ? "*" : "", p->get_move(npc_move_index));
  getch();
  if((rand() % 100) < p->get_move_accuracy(npc_move_index))
  {
    npc_damage = ((((2 * p->get_level() / 5) + 2) * p->get_move_power(npc_move_index) * (p->get_atk() / p->get_def()) /50) + 2) * critical * 3 * p->get_level() / 3; // no stab, no type
    if(critical == 1.5)
    {
      io_top_clear();
      mvprintw(0, 0, "It was a critical hit!");
      getch();
    }
  }
  else
  {
    npc_damage = 0;
    io_top_clear();
    mvprintw(0, 0, "It missed!");
    getch();
  }
  world.pc.current_party_hp[active_pokemon] -= npc_damage;
  if(world.pc.current_party_hp[active_pokemon] < 0) world.pc.current_party_hp[active_pokemon] = 0;
}

int io_damage(int active_pokemon, int move_index, pokemon *p, int current_wild_hp)
{
  int pc_damage = 0;
  int npc_damage = 0;
  int total_npc_moves = 0;
  int npc_move_index;
  int first; //0 = pc, 1 = npc
  double critical;
  total_npc_moves = 0;
  for(int i = 0; i < 4; i++)
  {
    if(p->get_move_accuracy(i) != -1)
    {
      total_npc_moves++;
    }
  }
  npc_move_index = rand() % total_npc_moves;
  if(world.pc.party[active_pokemon]->get_move_priority(move_index) > p->get_move_priority(npc_move_index)) first = 0;
  else if (world.pc.party[active_pokemon]->get_move_priority(move_index) < p->get_move_priority(npc_move_index)) first = 1;
  else
  {
    if(world.pc.party[active_pokemon]->get_speed() >= p->get_speed()) first = 0;
    else first = 1;
  }
  
  if(first == 0)
  {
    if((rand() % 100) <= 20) critical = 1.5;
    else critical = 1;
    io_top_clear();
    mvprintw(0, 0, "%s%s%s used %s!", world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_species(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_move(move_index));
    getch();
    if((rand() % 100) < world.pc.party[active_pokemon]->get_move_accuracy(move_index))
    {
      pc_damage = ((((2 * world.pc.party[active_pokemon]->get_level() / 5) + 2) * world.pc.party[active_pokemon]->get_move_power(move_index) * (world.pc.party[active_pokemon]->get_atk() / world.pc.party[active_pokemon]->get_def()) /50) + 2) * critical * 3 * world.pc.party[active_pokemon]->get_level() / 3; // no stab, no type
      if(critical == 1.5)
      {
        io_top_clear();
        mvprintw(0, 0, "It was a critical hit!");
        getch();
      }
    }
    else
    {
      pc_damage = 0;
      io_top_clear();
      mvprintw(0, 0, "It missed!");
      getch();
    }
    if(pc_damage < current_wild_hp)
    {
      if((rand() % 100) <= 20) critical = 1.5;
      else critical = 1;
      io_top_clear();
      mvprintw(0, 0, "The wild %s%s%s used %s!", p->is_shiny() ? "*" : "", p->get_species(),p->is_shiny() ? "*" : "", p->get_move(npc_move_index));
      getch();
      if((rand() % 100) < p->get_move_accuracy(move_index))
      {
        npc_damage = ((((2 * p->get_level() / 5) + 2) * p->get_move_power(move_index) * (p->get_atk() / p->get_def()) /50) + 2) * critical * 3 * p->get_level() / 3; // no stab, no type
        if(critical == 1.5)
        {
          io_top_clear();
          mvprintw(0, 0, "It was a critical hit!");
          getch();
        }
      }
      else
      {
        npc_damage = 0;
        io_top_clear();
        mvprintw(0, 0, "It missed!");
        getch();
      }
      world.pc.current_party_hp[active_pokemon] -= npc_damage;
      if(world.pc.current_party_hp[active_pokemon] < 0) world.pc.current_party_hp[active_pokemon] = 0;
    }
  }
  else
  {
    if((rand() % 100) <= 20) critical = 1.5;
    else critical = 1;
    io_top_clear();
    mvprintw(0, 0, "The wild %s%s%s used %s!", p->is_shiny() ? "*" : "", p->get_species(),p->is_shiny() ? "*" : "", p->get_move(npc_move_index));
    getch();
    if((rand() % 100) < p->get_move_accuracy(move_index))
    {
      npc_damage = ((((2 * p->get_level() / 5) + 2) * p->get_move_power(move_index) * (p->get_atk() / p->get_def()) /50) + 2) * critical * 3 * p->get_level() / 3; // no stab, no type
      if(critical == 1.5)
      {
        io_top_clear();
        mvprintw(0, 0, "It was a critical hit!");
        getch();
      }
    }
    else
    {
      npc_damage = 0;
      io_top_clear();
      mvprintw(0, 0, "It missed!");
      getch();
    }
    world.pc.current_party_hp[active_pokemon] -= npc_damage;
    if(world.pc.current_party_hp[active_pokemon] < 0) world.pc.current_party_hp[active_pokemon] = 0;
    if(world.pc.current_party_hp[active_pokemon] > 0)
    {
      if((rand() % 100) <= 20) critical = 1.5;
      else critical = 1;
      io_top_clear();
      mvprintw(0, 0, "%s%s%s used %s!", world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_species(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_move(move_index));
      getch();
      if((rand() % 100) < world.pc.party[active_pokemon]->get_move_accuracy(move_index))
      {
        pc_damage = ((((2 * world.pc.party[active_pokemon]->get_level() / 5) + 2) * world.pc.party[active_pokemon]->get_move_power(move_index) * (world.pc.party[active_pokemon]->get_atk() / world.pc.party[active_pokemon]->get_def()) /50) + 2) * critical * 3 * world.pc.party[active_pokemon]->get_level() / 3; // no stab, no type
        if(critical == 1.5)
        {
          io_top_clear();
          mvprintw(0, 0, "It was a critical hit!");
          getch();
        }
      }
      else
      {
        pc_damage = 0;
        io_top_clear();
        mvprintw(0, 0, "It missed!");
        getch();
      }
    }
  }  
  return pc_damage;
}

int io_wild_turn(int active_pokemon, pokemon *p, int current_wild_hp)
{
  int damage_dealt;
  int key;
  bool loop = true;
  while(loop)
  {
    io_top2_clear();
    mvprintw(1, 0, "%s %s %s %s %s %s %s %s", strcmp(world.pc.party[active_pokemon]->get_move(0),"") == 0 ? "" : "1.", world.pc.party[active_pokemon]->get_move(0), strcmp(world.pc.party[active_pokemon]->get_move(1), "") == 0 ? "               " : "2.", world.pc.party[active_pokemon]->get_move(1), strcmp(world.pc.party[active_pokemon]->get_move(2), "") == 0 ? "               " : "3.", world.pc.party[active_pokemon]->get_move(2), strcmp(world.pc.party[active_pokemon]->get_move(3), "") == 0 ? "" : "4.", world.pc.party[active_pokemon]->get_move(3));
    switch(key = getch())
    {
      case '1':
        if(world.pc.party[active_pokemon]->get_move_accuracy(0) != -1) 
        {
          damage_dealt = io_damage(active_pokemon, 0, p, current_wild_hp);
          loop = false;
        }
        break;
      case '2':
        if(world.pc.party[active_pokemon]->get_move_accuracy(1) != -1) 
        {
          damage_dealt = io_damage(active_pokemon, 1, p, current_wild_hp);
          loop = false;
        }
        break;
      case '3':
        if(world.pc.party[active_pokemon]->get_move_accuracy(2) != -1) 
        {
          damage_dealt = io_damage(active_pokemon, 2, p, current_wild_hp);
          loop = false;
        }
        break;
      case '4':
        if(world.pc.party[active_pokemon]->get_move_accuracy(3) != -1) 
        {
          damage_dealt = io_damage(active_pokemon, 3, p, current_wild_hp);
          loop = false;
        }
        break;
      default:
        break;
    }
  }
  return damage_dealt;
}

bool black_out()
{
  int useable_pokemon = 0;
  int money;
  for(int i = 0; i < 6; i++)
  {
    if(world.pc.current_party_hp[i] != 0 && world.pc.current_party_hp[i] != -1)
    {
      useable_pokemon++;
    }
  }
  if(useable_pokemon == 0)
  {
    io_top_clear();
    mvprintw(0, 0, "You ran out of useable Pokemon!");
    getch();
    money = rand() % 100 + 200;
    if((world.pc.wallet - money) < 0) money = world.pc.wallet;
    world.pc.wallet = 0;
    io_top_clear();
    mvprintw(0, 0, "You paid out $%d and rushed to the nearest Pokemon Center", money);
    getch();
    io_pokemon_center();
    return false;
  }
  return true;
}

int io_trainer_turn(int active_pokemon, pokemon *p, int current_wild_hp)
{
  int damage_dealt;
  int key;
  bool loop = true;
  while(loop)
  {
    io_top2_clear();
    mvprintw(1, 0, "%s %s %s %s %s %s %s %s", strcmp(world.pc.party[active_pokemon]->get_move(0),"") == 0 ? "" : "1.", world.pc.party[active_pokemon]->get_move(0), strcmp(world.pc.party[active_pokemon]->get_move(1), "") == 0 ? "               " : "2.", world.pc.party[active_pokemon]->get_move(1), strcmp(world.pc.party[active_pokemon]->get_move(2), "") == 0 ? "               " : "3.", world.pc.party[active_pokemon]->get_move(2), strcmp(world.pc.party[active_pokemon]->get_move(3), "") == 0 ? "" : "4.", world.pc.party[active_pokemon]->get_move(3));
    switch(key = getch())
    {
      case '1':
        if(world.pc.party[active_pokemon]->get_move_accuracy(0) != -1) 
        {
          damage_dealt = io_damage(active_pokemon, 0, p, current_wild_hp);
          loop = false;
        }
        break;
      case '2':
        if(world.pc.party[active_pokemon]->get_move_accuracy(1) != -1) 
        {
          damage_dealt = io_damage(active_pokemon, 1, p, current_wild_hp);
          loop = false;
        }
        break;
      case '3':
        if(world.pc.party[active_pokemon]->get_move_accuracy(2) != -1) 
        {
          damage_dealt = io_damage(active_pokemon, 2, p, current_wild_hp);
          loop = false;
        }
        break;
      case '4':
        if(world.pc.party[active_pokemon]->get_move_accuracy(3) != -1) 
        {
          damage_dealt = io_damage(active_pokemon, 3, p, current_wild_hp);
          loop = false;
        }
        break;
      default:
        break;
    }
  }
  return damage_dealt;
}

void io_battle(character *aggressor, character *defender)
{
  npc *n = (npc *) ((aggressor == &world.pc) ? defender : aggressor);



  n->defeated = 1;
  if (n->ctype == char_hiker || n->ctype == char_rival) {
    n->mtype = move_wander;
  }
}

void io_trainer_battle()
{
  pokemon *opp[6];
  int current_opp_hp[6];
  int pc_active_pokemon = 0;
  int opp_active_pokemon = 0;
  pokemon *p;
  int opp_total;
  int money;
  int md = (abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)) +
            abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)));
  int minl, maxl;

  if (md <= 200) {
    minl = 1;
    maxl = md / 2;
  } else {
    minl = (md - 200) / 2;
    maxl = 100;
  }
  if (minl < 1) {
    minl = 1;
  }
  if (minl > 100) {
    minl = 100;
  }
  if (maxl < 1) {
    maxl = 1;
  }
  if (maxl > 100) {
    maxl = 100;
  }
  opp_total = 1;
  for(int i = 0; i < 6; i++)
  {
    current_opp_hp[i] = -1;
    if(rand() % 100 <= 60) opp_total++;
  }
  for(int j = 0; j < opp_total; j++)
  {
    p = new pokemon(rand() % (maxl - minl + 1) + minl);
    opp[j] = (pokemon *) malloc(sizeof(*opp[j]));
    memcpy(opp[j], p, sizeof(*opp[j]));
    current_opp_hp[j] = p->get_hp();
  }
  //bool end = false;
  bool loop = true;
  bool loop2 = true;
  int key;
  int exp;
  int levels;
    io_top_clear();
    mvprintw(0, 0, "You were challenged by an opponent!");
    getch();
    io_top_clear();
    mvprintw(0, 0, "They sent out a %s%s%s!", opp[opp_active_pokemon]->is_shiny() ? "*" : "", opp[opp_active_pokemon]->get_species(), opp[opp_active_pokemon]->is_shiny() ? "*" : "");
    refresh();
    getch();
    while(world.pc.current_party_hp[pc_active_pokemon] == 0) pc_active_pokemon++;
    do
    {
      //print the current hp of both active pokemon
      io_top_clear();
      mvprintw(0, 0, "Your LV. %d %s%s%s: %d/%d Opponent's LV. %d %s%s%s: %d/%d", world.pc.party[pc_active_pokemon]->get_level(), world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "", world.pc.party[pc_active_pokemon]->get_species(), world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "", world.pc.current_party_hp[pc_active_pokemon],  world.pc.party[pc_active_pokemon]->get_hp(), opp[opp_active_pokemon]->get_level(), opp[opp_active_pokemon]->is_shiny() ? "*" : "", opp[opp_active_pokemon]->get_species(), opp[opp_active_pokemon]->is_shiny() ? "*" : "", current_opp_hp[opp_active_pokemon], opp[opp_active_pokemon]->get_hp());
      io_top2_clear();
      mvprintw(1, 0, "1. Fight 2. Bag 3. Run 4. Pokemon");
      switch(key = getch())
      {
        case '1': //Fight
          current_opp_hp[opp_active_pokemon] -= io_trainer_turn(pc_active_pokemon, opp[opp_active_pokemon], current_opp_hp[opp_active_pokemon]);
          //check both current hp to see if either is 0
          if(current_opp_hp[opp_active_pokemon] <= 0)
          {
            current_opp_hp[opp_active_pokemon] = 0;
            io_top_clear();
            mvprintw(0, 0, "The opponent's %s%s%s fainted!", opp[opp_active_pokemon]->is_shiny() ? "*" : "", opp[opp_active_pokemon]->get_species(), opp[opp_active_pokemon]->is_shiny() ? "*" : "");
            getch();
            io_top_clear();
            exp = (rand() % 75 + 10) * opp[opp_active_pokemon]->get_level();
            mvprintw(0, 0, "Your %s%s%s gained %d exp!", world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "", world.pc.party[pc_active_pokemon]->get_species(), world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "", exp);
            getch();
            levels = world.pc.party[pc_active_pokemon]->gain_exp(exp);
            if(levels > 0)
            {
              io_top_clear();
              mvprintw(0, 0, "It grew %d levels!", levels);
              getch();
            }
            if(opp_active_pokemon != (opp_total - 1))
            {
              opp_active_pokemon++;
              io_top_clear();
              mvprintw(0, 0, "The opponent sent out %s%s%s! They have %d remaining Pokemon!", opp[opp_active_pokemon]->is_shiny() ? "*" : "", opp[opp_active_pokemon]->get_species(), opp[opp_active_pokemon]->is_shiny() ? "*" : "", (opp_total - opp_active_pokemon));
              getch();
            }
            else
            {
              money = rand() % 400 + 150;
              io_top_clear();
              mvprintw(0, 0, "The opponent ran out of useable Pokemon! You win this battle!");
              io_display();
              getch();
              io_top_clear();
              mvprintw(0, 0, "Aww, how'd you get so strong?  You and your pokemon must share a special bond!");
              getch();
              io_top_clear();
              mvprintw(0, 0, "The opposing trainer paid out $%d.", money);
              refresh();
              getch();
              loop = false;
              world.pc.wallet += money;
            }
          }
          
            if(world.pc.current_party_hp[pc_active_pokemon] == 0)
            {
              io_top_clear();
              mvprintw(0, 0, "Your %s%s%s fainted!", world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "", world.pc.party[pc_active_pokemon]->get_species(), world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "");
              getch();
              loop = black_out();
              if(loop)
              {
                loop2 = true;
                while(loop2 == true)
                {
                  io_top_clear();
                  mvprintw(0, 0, "Choose a Pokemon to swap in.");
                  io_display_current_party();
                  switch(key = getch())
                  {
                    case '1':
                      if(world.pc.current_party_hp[0] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else
                      {
                        pc_active_pokemon = 0;
                        loop2 = false;
                      }
                      break;
                    case '2':
                      if(world.pc.party[1] != NULL)
                      {
                        pc_active_pokemon = 1;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[1] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '3': 
                      if(world.pc.party[2] != NULL)
                      {
                        pc_active_pokemon = 2;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[2] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true; 
                        getch();
                      }
                      break;
                    case '4':
                      if(world.pc.party[3] != NULL)
                      {
                        pc_active_pokemon = 3;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[3] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '5':
                      if(world.pc.party[4] != NULL)
                      {
                        pc_active_pokemon = 4;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[4] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '6':
                      if(world.pc.party[5] != NULL)
                      {
                        pc_active_pokemon = 5;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[5] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    default:
                      loop2 = true;
                      break;
                  }
                }
              }
            }
          
          break;
        case '2': //Bag
          io_display_bag_contents('2', NULL);
          io_opponent_attack(pc_active_pokemon, opp[opp_active_pokemon]);
          if(world.pc.current_party_hp[pc_active_pokemon] == 0)
            {
              io_top_clear();
              mvprintw(0, 0, "Your %s%s%s fainted!", world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "", world.pc.party[pc_active_pokemon]->get_species(), world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "");
              getch();
              loop = black_out();
              if(loop)
              {
                loop2 = true;
                while(loop2 == true)
                {
                  io_top_clear();
                  mvprintw(0, 0, "Choose a Pokemon to swap in.");
                  io_display_current_party();
                  switch(key = getch())
                  {
                    case '1':
                      if(world.pc.current_party_hp[0] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else
                      {
                        pc_active_pokemon = 0;
                        loop2 = false;
                      }
                      break;
                    case '2':
                      if(world.pc.party[1] != NULL)
                      {
                        pc_active_pokemon = 1;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[1] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '3': 
                      if(world.pc.party[2] != NULL)
                      {
                        pc_active_pokemon = 2;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[2] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true; 
                        getch();
                      }
                      break;
                    case '4':
                      if(world.pc.party[3] != NULL)
                      {
                        pc_active_pokemon = 3;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[3] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '5':
                      if(world.pc.party[4] != NULL)
                      {
                        pc_active_pokemon = 4;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[4] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '6':
                      if(world.pc.party[5] != NULL)
                      {
                        pc_active_pokemon = 5;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[5] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    default:
                      loop2 = true;
                      break;
                  }
                }
              }
            }
          break;
        case '3': //Run
          io_top_clear();
          mvprintw(0, 0, "Can't escape!");
          loop = true;
          getch();
          break;
        case '4': //Pokemon
          loop2 = true;
          while(loop2 == true)
                {
                  io_top_clear();
                  mvprintw(0, 0, "Choose a Pokemon to swap in.");
                  io_display_current_party();
                  switch(key = getch())
                  {
                    case '1':
                      if(world.pc.current_party_hp[0] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else
                      {
                        pc_active_pokemon = 0;
                        loop2 = false;
                      }
                      break;
                    case '2':
                      if(world.pc.party[1] != NULL)
                      {
                        pc_active_pokemon = 1;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[1] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '3': 
                      if(world.pc.party[2] != NULL)
                      {
                        pc_active_pokemon = 2;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[2] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true; 
                        getch();
                      }
                      break;
                    case '4':
                      if(world.pc.party[3] != NULL)
                      {
                        pc_active_pokemon = 3;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[3] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '5':
                      if(world.pc.party[4] != NULL)
                      {
                        pc_active_pokemon = 4;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[4] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '6':
                      if(world.pc.party[5] != NULL)
                      {
                        pc_active_pokemon = 5;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[5] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    default:
                      loop2 = true;
                      break;
                  }
                }
          io_opponent_attack(pc_active_pokemon, opp[opp_active_pokemon]);
          if(world.pc.current_party_hp[pc_active_pokemon] == 0)
            {
              io_top_clear();
              mvprintw(0, 0, "Your %s%s%s fainted!", world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "", world.pc.party[pc_active_pokemon]->get_species(), world.pc.party[pc_active_pokemon]->is_shiny() ? "*" : "");
              getch();
              loop = black_out();
              if(loop)
              {
                loop2 = true;
                while(loop2 == true)
                {
                  io_top_clear();
                  mvprintw(0, 0, "Choose a Pokemon to swap in.");
                  io_display_current_party();
                  switch(key = getch())
                  {
                    case '1':
                      if(world.pc.current_party_hp[0] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else
                      {
                        pc_active_pokemon = 0;
                        loop2 = false;
                      }
                      break;
                    case '2':
                      if(world.pc.party[1] != NULL)
                      {
                        pc_active_pokemon = 1;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[1] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '3': 
                      if(world.pc.party[2] != NULL)
                      {
                        pc_active_pokemon = 2;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[2] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true; 
                        getch();
                      }
                      break;
                    case '4':
                      if(world.pc.party[3] != NULL)
                      {
                        pc_active_pokemon = 3;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[3] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '5':
                      if(world.pc.party[4] != NULL)
                      {
                        pc_active_pokemon = 4;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[4] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '6':
                      if(world.pc.party[5] != NULL)
                      {
                        pc_active_pokemon = 5;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[5] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    default:
                      loop2 = true;
                      break;
                  }
                }
              }
            }
          loop2 = true;
          break;
        default:
          loop = true;
          break;
      }
    }while(loop);
  //}
  
  // Later on, don't delete if captured
  delete p;
}

uint32_t move_pc_dir(uint32_t input, pair_t dest)
{
  dest[dim_y] = world.pc.pos[dim_y];
  dest[dim_x] = world.pc.pos[dim_x];

  switch (input) {
  case 1:
  case 2:
  case 3:
    dest[dim_y]++;
    break;
  case 4:
  case 5:
  case 6:
    break;
  case 7:
  case 8:
  case 9:
    dest[dim_y]--;
    break;
  }
  switch (input) {
  case 1:
  case 4:
  case 7:
    dest[dim_x]--;
    break;
  case 2:
  case 5:
  case 8:
    break;
  case 3:
  case 6:
  case 9:
    dest[dim_x]++;
    break;
  case '>':
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_mart) {
      io_pokemart();
    }
    if (world.cur_map->map[world.pc.pos[dim_y]][world.pc.pos[dim_x]] ==
        ter_center) {
      io_pokemon_center();
    }
    break;
  }

  if (world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) {
    if (dynamic_cast<npc *>(world.cur_map->cmap[dest[dim_y]][dest[dim_x]]) &&
        ((npc *) world.cur_map->cmap[dest[dim_y]][dest[dim_x]])->defeated) {
      // Some kind of greeting here would be nice
      return 1;
    } else if (dynamic_cast<npc *>
               (world.cur_map->cmap[dest[dim_y]][dest[dim_x]])) {
      io_trainer_battle();
      io_battle(&world.pc, world.cur_map->cmap[dest[dim_y]][dest[dim_x]]);
      
      // Not actually moving, so set dest back to PC position
      dest[dim_x] = world.pc.pos[dim_x];
      dest[dim_y] = world.pc.pos[dim_y];
    }
  }
  
  if (move_cost[char_pc][world.cur_map->map[dest[dim_y]][dest[dim_x]]] ==
      INT_MAX) {
    return 1;
  }

  return 0;
}



void io_teleport_world(pair_t dest)
{
  /* mvscanw documentation is unclear about return values.  I believe *
   * that the return value works the same way as scanf, but instead   *
   * of counting on that, we'll initialize x and y to out of bounds   *
   * values and accept their updates only if in range.                */
  int x = INT_MAX, y = INT_MAX;
  
  world.cur_map->cmap[world.pc.pos[dim_y]][world.pc.pos[dim_x]] = NULL;

  echo();
  curs_set(1);
  do {
    mvprintw(0, 0, "Enter x [-200, 200]:           ");
    refresh();
    mvscanw(0, 21, "%d", &x);
  } while (x < -200 || x > 200);
  do {
    mvprintw(0, 0, "Enter y [-200, 200]:          ");
    refresh();
    mvscanw(0, 21, "%d", &y);
  } while (y < -200 || y > 200);

  refresh();
  noecho();
  curs_set(0);

  x += 200;
  y += 200;

  world.cur_idx[dim_x] = x;
  world.cur_idx[dim_y] = y;

  new_map(1);
  io_teleport_pc(dest);
}

void io_handle_input(pair_t dest)
{
  uint32_t turn_not_consumed;
  int key;

  do {
    switch (key = getch()) {
    case '7':
    case 'y':
    case KEY_HOME:
      turn_not_consumed = move_pc_dir(7, dest);
      break;
    case '8':
    case 'k':
    case KEY_UP:
      turn_not_consumed = move_pc_dir(8, dest);
      break;
    case '9':
    case 'u':
    case KEY_PPAGE:
      turn_not_consumed = move_pc_dir(9, dest);
      break;
    case '6':
    case 'l':
    case KEY_RIGHT:
      turn_not_consumed = move_pc_dir(6, dest);
      break;
    case '3':
    case 'n':
    case KEY_NPAGE:
      turn_not_consumed = move_pc_dir(3, dest);
      break;
    case '2':
    case 'j':
    case KEY_DOWN:
      turn_not_consumed = move_pc_dir(2, dest);
      break;
    case '1':
    case 'b':
    case KEY_END:
      turn_not_consumed = move_pc_dir(1, dest);
      break;
    case '4':
    case 'h':
    case KEY_LEFT:
      turn_not_consumed = move_pc_dir(4, dest);
      break;
    case '5':
    case ' ':
    case '.':
    case KEY_B2:
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    case '>':
      turn_not_consumed = move_pc_dir('>', dest);
      break;
    case 'Q':
      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      world.quit = 1;
      turn_not_consumed = 0;
      break;
      break;
    case 't':
      io_list_trainers();
      turn_not_consumed = 1;
      break;
    case 'p':
      /* Teleport the PC to a random place in the map.               */
      io_teleport_pc(dest);
      turn_not_consumed = 0;
      break;
    case 'm':
      io_list_trainers();
      turn_not_consumed = 1;
      break;
    case 'f':
      /* Fly to any map in the world.                                */
      io_teleport_world(dest);
      turn_not_consumed = 0;
      break;
    case 'q':
      /* Demonstrate use of the message queue.  You can use this for *
       * printf()-style debugging (though gdb is probably a better   *
       * option.  Not that it matters, but using this command will   *
       * waste a turn.  Set turn_not_consumed to 1 and you should be *
       * able to figure out why I did it that way.                   */
      io_queue_message("This is the first message.");
      io_queue_message("Since there are multiple messages, "
                       "you will see \"more\" prompts.");
      io_queue_message("You can use any key to advance through messages.");
      io_queue_message("Normal gameplay will not resume until the queue "
                       "is empty.");
      io_queue_message("Long lines will be truncated, not wrapped.");
      io_queue_message("io_queue_message() is variadic and handles "
                       "all printf() conversion specifiers.");
      io_queue_message("Did you see %s?", "what I did there");
      io_queue_message("When the last message is displayed, there will "
                       "be no \"more\" prompt.");
      io_queue_message("Have fun!  And happy printing!");
      io_queue_message("Oh!  And use 'Q' to quit!");

      dest[dim_y] = world.pc.pos[dim_y];
      dest[dim_x] = world.pc.pos[dim_x];
      turn_not_consumed = 0;
      break;
    case 'B':
      io_display_bag_contents('0', NULL);
      turn_not_consumed = 1;
      break;
    case 'P':
      io_display_current_party();
      turn_not_consumed = 1;
      getch();
      break;
    default:
      /* Also not in the spec.  It's not always easy to figure out what *
       * key code corresponds with a given keystroke.  Print out any    *
       * unhandled key here.  Not only does it give a visual error      *
       * indicator, but it also gives an integer value that can be used *
       * for that key in this (or other) switch statements.  Printed in *
       * octal, with the leading zero, because ncurses.h lists codes in *
       * octal, thus allowing us to do reverse lookups.  If a key has a *
       * name defined in the header, you can use the name here, else    *
       * you can directly use the octal value.                          */
      mvprintw(0, 0, "Unbound key: %#o ", key);
      turn_not_consumed = 1;
    }
    refresh();
  } while (turn_not_consumed);
}



void io_encounter_pokemon()
{
  pokemon *p;
  int md = (abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)) +
            abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)));
  int minl, maxl;
  int exp;
  if (md <= 200) {
    minl = 1;
    maxl = md / 2;
  } else {
    minl = (md - 200) / 2;
    maxl = 100;
  }
  if (minl < 1) {
    minl = 1;
  }
  if (minl > 100) {
    minl = 100;
  }
  if (maxl < 1) {
    maxl = 1;
  }
  if (maxl > 100) {
    maxl = 100;
  }

  p = new pokemon(rand() % (maxl - minl + 1) + minl);

  //bool end = false;
  bool loop = true;
  bool loop2 = true;
  bool caught = false;
  int key;
  int levels;
  int current_wild_hp = p->get_hp();
  int active_pokemon = 0;
  int attempts = 1;
  while(world.pc.current_party_hp[active_pokemon] == 0) active_pokemon++;
    mvprintw(0, 0, "A wild %s%s%s appeared!", p->is_shiny() ? "*" : "", p->get_species(), p->is_shiny() ? "*" : "");
    refresh();
    getch();
    do
    {
      //print the current hp of both active pokemon
      io_top_clear();
      mvprintw(0, 0, "Your LV. %d %s%s%s: %d/%d Wild LV. %d %s%s%s: %d/%d", world.pc.party[active_pokemon]->get_level(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_species(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.current_party_hp[active_pokemon],  world.pc.party[active_pokemon]->get_hp(), p->get_level(), p->is_shiny() ? "*" : "", p->get_species(), p->is_shiny() ? "*" : "", current_wild_hp, p->get_hp());
      io_top2_clear();
      mvprintw(1, 0, "1. Fight 2. Bag 3. Run 4. Pokemon");
      switch(key = getch())
      {
        case '1': //Fight
          current_wild_hp -= io_wild_turn(active_pokemon, p, current_wild_hp);
          //check both current hp to see if either is 0
          if(current_wild_hp <= 0)
          {
            current_wild_hp = 0;
            io_top_clear();
            mvprintw(0, 0, "The wild %s%s%s fainted!", p->is_shiny() ? "*" : "", p->get_species(), p->is_shiny() ? "*" : "");
            getch();
            io_top_clear();
            exp = (rand() % 75 + 10) * p->get_level();
            mvprintw(0, 0, "Your %s%s%s gained %d exp!", world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_species(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "", exp);
            getch();
            levels = world.pc.party[active_pokemon]->gain_exp(exp);
            if(levels > 0)
            {
              io_top_clear();
              mvprintw(0, 0, "It grew %d levels!", levels);
              getch();
            }
            loop = false;
          }
          else
          {
            if(world.pc.current_party_hp[active_pokemon] == 0)
            {
              io_top_clear();
              mvprintw(0, 0, "Your %s%s%s fainted!", world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_species(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "");
              getch();
              loop = black_out();
              if(loop)
              {
                loop2 = true;
                while(loop2 == true)
                {
                  io_top_clear();
                  mvprintw(0, 0, "Choose a Pokemon to swap in.");
                  io_display_current_party();
                  switch(key = getch())
                  {
                    case '1':
                      if(world.pc.current_party_hp[0] == 0)
                    {
                      io_top2_clear();
                      mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
                      getch();
                    }
                    else
                    {
                      active_pokemon = 0;
                      loop2 = false;
                    }
                      break;
                    case '2':
                      if(world.pc.party[1] != NULL)
                      {
                        active_pokemon = 1;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[1] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '3': 
                      if(world.pc.party[2] != NULL)
                      {
                        active_pokemon = 2;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[2] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true; 
                        getch();
                      }
                      break;
                    case '4':
                      if(world.pc.party[3] != NULL)
                      {
                        active_pokemon = 3;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[3] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '5':
                      if(world.pc.party[4] != NULL)
                      {
                        active_pokemon = 4;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[4] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '6':
                      if(world.pc.party[5] != NULL)
                      {
                        active_pokemon = 5;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[5] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    default:
                      loop2 = true;
                      break;
                  }
                }
              }
            }
          }
          break;
        case '2': //Bag
          caught = io_display_bag_contents('1', p);
          if(caught == false)
          {
            io_opponent_attack(active_pokemon, p);
            if(world.pc.current_party_hp[active_pokemon] == 0)
            {
              io_top_clear();
              mvprintw(0, 0, "Your %s%s%s fainted!", world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_species(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "");
              getch();
              loop = black_out();
              if(loop)
              {
                loop2 = true;
                while(loop2 == true)
                {
                  io_top_clear();
                  mvprintw(0, 0, "Choose a Pokemon to swap in.");
                  io_display_current_party();
                  switch(key = getch())
                  {
                    case '1':
                    if(world.pc.current_party_hp[0] == 0)
                    {
                      io_top2_clear();
                      mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
                      getch();
                    }
                    else
                    {
                      active_pokemon = 0;
                      loop2 = false;
                    }
                      break;
                    case '2':
                      if(world.pc.party[1] != NULL)
                      {
                        active_pokemon = 1;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[1] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '3': 
                      if(world.pc.party[2] != NULL)
                      {
                        active_pokemon = 2;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[2] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true; 
                        getch();
                      }
                      break;
                    case '4':
                      if(world.pc.party[3] != NULL)
                      {
                        active_pokemon = 3;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[3] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '5':
                      if(world.pc.party[4] != NULL)
                      {
                        active_pokemon = 4;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[4] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '6':
                      if(world.pc.party[5] != NULL)
                      {
                        active_pokemon = 5;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[5] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    default:
                      loop2 = true;
                      break;
                  }
                }
              }
            }
          }
          break;
        case '3': //Run
          if(rand() % 256 < ((world.pc.party[active_pokemon]->get_speed() * 32)/((p->get_speed()/4) % 256) + 30 * attempts))
          {
            loop = false;
            refresh();
            io_top_clear();
            mvprintw(0, 0, "%s%s%s successfully fled!", world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_species(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "");
            getch();
          }  
          else
          {
            refresh();
            io_top_clear();
            mvprintw(0, 0, "Can't escape!");
            loop = true;
            refresh();
            getch();
            attempts++;
            io_opponent_attack(active_pokemon, p);
            if(world.pc.current_party_hp[active_pokemon] == 0)
            {
              io_top_clear();
              mvprintw(0, 0, "Your %s%s%s fainted!", world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_species(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "");
              getch();
              loop = black_out();
              if(loop)
              {
                loop2 = true;
                while(loop2 == true)
                {
                  io_top_clear();
                  mvprintw(0, 0, "Choose a Pokemon to swap in.");
                  io_display_current_party();
                  switch(key = getch())
                  {
                    case '1':
                    if(world.pc.current_party_hp[0] == 0)
                    {
                      io_top2_clear();
                      mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
                      getch();
                    }
                    else
                    {
                      active_pokemon = 0;
                      loop2 = false;
                    }
                      break;
                    case '2':
                      if(world.pc.party[1] != NULL)
                      {
                        active_pokemon = 1;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[1] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '3': 
                      if(world.pc.party[2] != NULL)
                      {
                        active_pokemon = 2;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[2] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true; 
                        getch();
                      }
                      break;
                    case '4':
                      if(world.pc.party[3] != NULL)
                      {
                        active_pokemon = 3;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[3] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '5':
                      if(world.pc.party[4] != NULL)
                      {
                        active_pokemon = 4;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[4] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '6':
                      if(world.pc.party[5] != NULL)
                      {
                        active_pokemon = 5;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[5] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    default:
                      loop2 = true;
                      break;
                  }
                }
              }
            }
          }
          break;
        case '4': //Pokemon
          loop2 = true;
          while(loop2 == true)
          {
            io_top_clear();
            mvprintw(0, 0, "Choose a Pokemon to swap in.");
            io_display_current_party();
            switch(key = getch())
            {
              case '1':
                if(world.pc.current_party_hp[0] == 0)
                    {
                      io_top2_clear();
                      mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
                      getch();
                    }
                    else
                    {
                      active_pokemon = 0;
                      loop2 = false;
                    }
                break;
              case '2':
                if(world.pc.party[1] != NULL)
                {
                  active_pokemon = 1;
                  loop2 = false;
                }
                else if(world.pc.current_party_hp[1] == 0)
                {
                  io_top2_clear();
                  mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
                  getch();
                }
                else 
                {
                  io_top2_clear();
                  mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                  loop2 = true;
                  getch();
                }
                break;
              case '3': 
                if(world.pc.party[2] != NULL)
                {
                  active_pokemon = 2;
                  loop2 = false;
                }
                else if(world.pc.current_party_hp[2] == 0)
                {
                  io_top2_clear();
                  mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
                  getch();
                }
                else 
                {
                  io_top2_clear();
                  mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                  loop2 = true; 
                  getch();
                }
                break;
              case '4':
                if(world.pc.party[3] != NULL)
                {
                  active_pokemon = 3;
                  loop2 = false;
                }
                else if(world.pc.current_party_hp[3] == 0)
                {
                  io_top2_clear();
                  mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
                  getch();
                }
                else 
                {
                  io_top2_clear();
                  mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                  loop2 = true;
                  getch();
                }
                break;
              case '5':
                if(world.pc.party[4] != NULL)
                {
                  active_pokemon = 4;
                  loop2 = false;
                }
                else if(world.pc.current_party_hp[4] == 0)
                {
                  io_top2_clear();
                  mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
                  getch();
                }
                else 
                {
                  io_top2_clear();
                  mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                  loop2 = true;
                  getch();
                }
                break;
              case '6':
                if(world.pc.party[5] != NULL)
                {
                  active_pokemon = 5;
                  loop2 = false;
                }
                else if(world.pc.current_party_hp[5] == 0)
                {
                  io_top2_clear();
                  mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
                  getch();
                }
                else 
                {
                  io_top2_clear();
                  mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                  loop2 = true;
                  getch();
                }
                break;
              default:
                loop2 = true;
                break;
            }
          }
          io_opponent_attack(active_pokemon, p);
            if(world.pc.current_party_hp[active_pokemon] == 0)
            {
              io_top_clear();
              mvprintw(0, 0, "Your %s%s%s fainted!", world.pc.party[active_pokemon]->is_shiny() ? "*" : "", world.pc.party[active_pokemon]->get_species(), world.pc.party[active_pokemon]->is_shiny() ? "*" : "");
              getch();
              loop = black_out();
              if(loop)
              {
                loop2 = true;
                while(loop2 == true)
                {
                  io_top_clear();
                  mvprintw(0, 0, "Choose a Pokemon to swap in.");
                  io_display_current_party();
                  switch(key = getch())
                  {
                    case '1':
                      if(world.pc.current_party_hp[0] == 0)
                    {
                      io_top2_clear();
                      mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[0]->is_shiny() ? "*" : "", world.pc.party[0]->get_species(), world.pc.party[0]->is_shiny() ? "*" : "");
                      getch();
                    }
                    else
                    {
                      active_pokemon = 0;
                      loop2 = false;
                    }
                      break;
                    case '2':
                      if(world.pc.party[1] != NULL)
                      {
                        active_pokemon = 1;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[1] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[1]->is_shiny() ? "*" : "", world.pc.party[1]->get_species(), world.pc.party[1]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '3': 
                      if(world.pc.party[2] != NULL)
                      {
                        active_pokemon = 2;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[2] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[2]->is_shiny() ? "*" : "", world.pc.party[2]->get_species(), world.pc.party[2]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true; 
                        getch();
                      }
                      break;
                    case '4':
                      if(world.pc.party[3] != NULL)
                      {
                        active_pokemon = 3;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[3] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[3]->is_shiny() ? "*" : "", world.pc.party[3]->get_species(), world.pc.party[3]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '5':
                      if(world.pc.party[4] != NULL)
                      {
                        active_pokemon = 4;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[4] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[4]->is_shiny() ? "*" : "", world.pc.party[4]->get_species(), world.pc.party[4]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    case '6':
                      if(world.pc.party[5] != NULL)
                      {
                        active_pokemon = 5;
                        loop2 = false;
                      }
                      else if(world.pc.current_party_hp[5] == 0)
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "%s%s%s is unconscious. Choose a different Pokemon to switch in!", world.pc.party[5]->is_shiny() ? "*" : "", world.pc.party[5]->get_species(), world.pc.party[5]->is_shiny() ? "*" : "");
                        getch();
                      }
                      else 
                      {
                        io_top2_clear();
                        mvprintw(1, 0, "Invalid input. No Pokemon in that slot.");
                        loop2 = true;
                        getch();
                      }
                      break;
                    default:
                      loop2 = true;
                      break;
                  }
                }
              }
            }
          loop2 = true;
          break;
        default:
          loop = true;
          break;
      }
    }while(loop && !caught);
  //}
  
  // Later on, don't delete if captured
  delete p;
}

int io_forget_a_move(const char *species, const char *move1, const char *move2, const char *move3, const char *move4, const char *move5)
{
  int key;
  bool loop = true;
  while(loop)
  {
    io_top_clear();
    io_top2_clear();
    mvprintw(0, 0, "Your %s wants to learn %s. Forget a move?", species, move5);
    mvprintw(1, 0, "1. %s 2. %s 3. %s 4. %s 5. Keep old moves", move1, move2, move3, move4);
    switch(key = getch())
    {
      case '1':
        io_top_clear();
        mvprintw(0, 0, "%s forgot %s and learned %s!", species, move1, move5);
        getch();
        return 0;
        break;
      case '2':
        io_top_clear();
        mvprintw(0, 0, "%s forgot %s and learned %s!", species, move2, move5);
        getch();
        return 1;
        break;
      case '3':
        io_top_clear();
        mvprintw(0, 0, "%s forgot %s and learned %s!", species, move3, move5);
        getch();
        return 2;
        break;
      case '4':
        io_top_clear();
        mvprintw(0, 0, "%s forgot %s and learned %s!", species, move4, move5);
        getch();
        return 3;
        break;
      case '5': //keep old moves
        io_top_clear();
        mvprintw(0, 0, "%s forgot %s and kept its old moves!", species, move5);
        getch();
        return -1;
        break;
      default:      
        break;
    }
  }
  return -1;
}
