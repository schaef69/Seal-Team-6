/*
 * +========================================+
 * |     *** TERMINAL ROULETTE ***          |
 * +========================================+
 *
 *  Compile:  gcc -o roulette roulette.c
 *  Run:      ./roulette
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

/* ---- Configuration ---------------------------------------------------- */
#define MAX_PLAYERS     8
#define MAX_NAME        32
#define MAX_BETS        10
#define STARTING_BAL    100.0

/* ---- ANSI colours ------------------------------------------------------ */
#define RST     "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define WHITE   "\033[97m"
#define BG_RED  "\033[41m"
#define BG_GRN  "\033[42m"
#define BG_BLK  "\033[40;97m"

/* ---- American wheel order (37 = 00) ----------------------------------- */
static const int WHEEL[38] = {
     0, 28,  9, 26, 30, 11,  7, 20, 32, 17,
     5, 22, 34, 15,  3, 24, 36, 13,  1, 37,
    27, 10, 25, 29, 12,  8, 19, 31, 18,  6,
    21, 33, 16,  4, 23, 35, 14,  2
};

static const int RED_NUMS[18] = {
    1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36
};

/* ---- Types ------------------------------------------------------------- */
typedef enum { COL_RED, COL_BLACK, COL_GREEN } Color;
typedef enum { BET_NUMBER, BET_COLOR, BET_ODD_EVEN, BET_HIGH_LOW } BetType;

typedef struct {
    BetType type;
    double  amount;
    int     number;
    int     value;
    char    label[24];
} Bet;

typedef struct {
    char   name[MAX_NAME];
    double balance;
    int    active;
    Bet    bets[MAX_BETS];
    int    nbet;
} Player;

/* ---- Helpers ----------------------------------------------------------- */
static void flush_stdin(void) { int c; while ((c=getchar())!='\n'&&c!=EOF); }

static Color num_color(int n) {
    if (n == 0 || n == 37) return COL_GREEN;
    for (int i = 0; i < 18; i++) if (RED_NUMS[i] == n) return COL_RED;
    return COL_BLACK;
}

/* print a single pocket inline */
static void print_cell(int n) {
    Color c = num_color(n);
    const char *bg = (c==COL_RED) ? BG_RED : (c==COL_GREEN) ? BG_GRN : BG_BLK;
    if (n == 37) printf("%s%s 00 %s", BOLD, bg, RST);
    else         printf("%s%s %2d %s", BOLD, bg, n, RST);
}

/* ====================================================================== *
 *  WHEEL ANIMATION                                                        *
 *                                                                         *
 *  We draw a 13-slot circular arc centred on the current "top" pocket.   *
 *  The arc is printed as two rows:                                        *
 *    row 0  (top rim)    – 5 pockets                                      *
 *    row 1  (mid-left)   – 1 pocket   [landing zone]   1 pocket (mid-rt) *
 *    row 2  (bottom rim) – 5 pockets                                      *
 *  ... giving a rough oval wheel feel, all in ASCII.                      *
 * ====================================================================== */

/*
 * Full wheel display  – 13 pockets visible at once arranged like:
 *
 *          [ p1 p2 p3 p4 p5 ]       <- top arc  (5 pockets)
 *       [p0]                [p6]    <- sides     (2 pockets)
 *          [ p7 p8 p9 p10 p11]      <- bottom arc(5 pockets)
 *                  ^
 *               LANDING  (pocket at position 6 in the side display,
 *                          but we highlight the TOP CENTRE pocket p2)
 *
 * We keep it simpler: draw 3 rows.  Rotate an index into WHEEL.
 */


/* Cleaner full-wheel ASCII art – one function, printed with cursor control */
static void draw_wheel2(int top) {
    /* top = index into WHEEL[] sitting at 12-o'clock */
    int w = 38;
#define WN(off) WHEEL[((top)+(off)+w*100)%w]

    /* We show a 17-pocket cross-section:
     *   top arc    : -4 -3 -2 -1  0  1  2  3  4   (9 pockets)
     *   left side  : -5                          5
     *   bottom arc :  6  7  8  9 10 11 12 13 14  (reversed so the wheel looks round)
     * = 21 pockets visible (half the wheel)
     * But terminal width matters; let's do 9 top / 2 sides / 9 bottom
     */

    /* ---- top rim ---- */
    printf("    .---");
    for (int i=0;i<9;i++) printf("-----");
    printf("---.\n");

    /* ---- top arc (9 pockets) ---- */
    printf("   /  ");
    for (int i=-4;i<=4;i++) print_cell(WN(i));
    printf("  \\\n");

    /* ---- middle row: left pocket, big gap (ball area), right pocket ---- */
    printf("  |  ");
    print_cell(WN(-5));
    printf("                                     ");
    print_cell(WN(5));
    printf("  |\n");

    /* ---- bottom arc (9 pockets, positions +14 down to +6, i.e. far side) ---- */
    printf("   \\  ");
    for (int i=14;i>=6;i--) print_cell(WN(i));
    printf("  /\n");

    /* ---- bottom rim ---- */
    printf("    '---");
    for (int i=0;i<9;i++) printf("-----");
    printf("---'\n");

#undef WN
}

/* Move cursor up N lines */
static void cursor_up(int n) { printf("\033[%dA", n); }

/* ---- Full spin animation ---------------------------------------------- */
static int spin_wheel(void) {
    int landing_idx = rand() % 38;

    /* Phase 1: full wheel spinning fast -> slow */
    static const int WHEEL_DELAYS[] = {
        50, 55, 60, 68, 78, 90, 105, 122, 142,
        165, 190, 220, 255, 295, 340
    };
    int nd = (int)(sizeof(WHEEL_DELAYS)/sizeof(WHEEL_DELAYS[0]));
    int total_frames = 42;

    printf("\n  " BOLD YELLOW "*** Spinning the wheel... ***\n\n" RST);
    fflush(stdout);

    /* Draw initial wheel (7 lines tall) so we have lines to overwrite */
    int wheel_lines = 7;
    draw_wheel2(0);

    for (int f = 0; f < total_frames; f++) {
        /* top-of-wheel index drifts toward landing */
        int top = ((landing_idx - (total_frames - f)) % 38 + 38 * 4) % 38;

        cursor_up(wheel_lines);
        draw_wheel2(top);
        fflush(stdout);

        int delay = (f < nd) ? WHEEL_DELAYS[f]
                              : WHEEL_DELAYS[nd-1] + (f-nd+1)*35;
        usleep((unsigned)(delay * 1000));
    }

    /* Phase 2: landing strip scroll – ball bouncing to final pocket */
    printf("\n  Ball settling...\n\n");
    fflush(stdout);

    static const int STRIP_DELAYS[] = {
        80, 95, 115, 140, 170, 205, 245, 290, 340, 400
    };
    int ns = (int)(sizeof(STRIP_DELAYS)/sizeof(STRIP_DELAYS[0]));
    int strip_frames = 18;

    for (int f = 0; f < strip_frames; f++) {
        int pos  = ((landing_idx - (strip_frames - f)) % 38 + 38*4) % 38;
        int prev = (pos - 1 + 38) % 38;
        int next = (pos + 1) % 38;

        printf("\r  ");
        print_cell(WHEEL[(prev-1+38)%38]);
        printf(" ");
        print_cell(WHEEL[prev]);
        printf("  " BOLD WHITE ">>[" RST " ");
        print_cell(WHEEL[pos]);
        printf(" " BOLD WHITE "]]<" RST "  ");
        print_cell(WHEEL[next]);
        printf(" ");
        print_cell(WHEEL[(next+1)%38]);
        printf("   ");
        fflush(stdout);

        int delay = (f < ns) ? STRIP_DELAYS[f]
                              : STRIP_DELAYS[ns-1] + (f-ns+1)*50;
        usleep((unsigned)(delay * 1000));
    }

    int result = WHEEL[landing_idx];
    Color rc   = num_color(result);
    const char *cname =
        (rc==COL_RED)   ? RED   "RED"   RST :
        (rc==COL_GREEN) ? GREEN "GREEN" RST :
                          WHITE "BLACK" RST;

    printf("\n\n  " BOLD "Ball lands on: ");
    print_cell(result);
    printf("  ( %s )\n\n" RST, cname);
    return result;
}

/* ---- Evaluate a bet --------------------------------------------------- */
static double eval_bet(const Bet *b, int result) {
    Color rc = num_color(result);
    switch (b->type) {
    case BET_NUMBER:
        return (b->number == result) ? b->amount * 35.0 : -b->amount;
    case BET_COLOR:
        if (rc == COL_GREEN) return -b->amount;
        return ((b->value==0 && rc==COL_RED)||(b->value==1 && rc==COL_BLACK))
               ? b->amount : -b->amount;
    case BET_ODD_EVEN:
        if (result==0||result==37) return -b->amount;
        return ((b->value==1 && result%2==1)||(b->value==0 && result%2==0))
               ? b->amount : -b->amount;
    case BET_HIGH_LOW:
        if (result<1||result>36) return -b->amount;
        return ((b->value==1 && result>=19)||(b->value==0 && result<=18))
               ? b->amount : -b->amount;
    }
    return -b->amount;
}

/* ---- Collect bets for one player -------------------------------------- */
static void collect_bets(Player *p) {
    p->nbet = 0;
    printf("\n" CYAN BOLD "  -- %s  (Balance: $%.2f) --\n" RST,
           p->name, p->balance);

    for (;;) {
        if (p->nbet >= MAX_BETS) { printf("  Max bets reached.\n"); break; }

        printf("\n"
               "  Bet type:\n"
               "    1) Number 0-36 / 00     pays 35:1\n"
               "    2) Red / Black          pays  1:1\n"
               "    3) Odd / Even           pays  1:1\n"
               "    4) Low 1-18 / High 19-36  pays 1:1\n"
               "    0) Done\n"
               "  > ");
        fflush(stdout);

        int choice;
        if (scanf("%d", &choice) != 1) { flush_stdin(); continue; }
        flush_stdin();
        if (choice == 0) break;
        if (choice < 1 || choice > 4) { printf("  Invalid.\n"); continue; }

        double amount;
        printf("  Amount ($0.01 - $%.2f): $", p->balance);
        fflush(stdout);
        if (scanf("%lf", &amount) != 1) { flush_stdin(); continue; }
        flush_stdin();
        if (amount <= 0.0 || amount > p->balance) {
            printf("  " RED "Invalid amount." RST "\n"); continue;
        }

        Bet b; memset(&b, 0, sizeof(b));
        b.amount = amount;
        int ok = 1;

        if (choice == 1) {
            b.type = BET_NUMBER;
            char buf[8];
            printf("  Number (0-36, or 00): ");
            fflush(stdout);
            if (scanf("%7s", buf) != 1) { flush_stdin(); continue; }
            flush_stdin();
            if (strcmp(buf,"00")==0) {
                b.number = 37; snprintf(b.label,sizeof(b.label),"00");
            } else {
                for (int i=0;buf[i];i++) if(!isdigit((unsigned char)buf[i])){ok=0;break;}
                int n = atoi(buf);
                if (n<0||n>36) ok=0;
                if (ok){ b.number=n; snprintf(b.label,sizeof(b.label),"%d",n); }
            }
        } else if (choice == 2) {
            b.type = BET_COLOR;
            char c[4];
            printf("  Red or Black? (r/b): ");
            fflush(stdout);
            if (scanf("%3s",c)!=1){flush_stdin();continue;}
            flush_stdin();
            char ch=tolower((unsigned char)c[0]);
            if      (ch=='r'){b.value=0;strcpy(b.label,"Red");}
            else if (ch=='b'){b.value=1;strcpy(b.label,"Black");}
            else ok=0;
        } else if (choice == 3) {
            b.type = BET_ODD_EVEN;
            char c[4];
            printf("  Odd or Even? (o/e): ");
            fflush(stdout);
            if (scanf("%3s",c)!=1){flush_stdin();continue;}
            flush_stdin();
            char ch=tolower((unsigned char)c[0]);
            if      (ch=='o'){b.value=1;strcpy(b.label,"Odd");}
            else if (ch=='e'){b.value=0;strcpy(b.label,"Even");}
            else ok=0;
        } else {
            b.type = BET_HIGH_LOW;
            char c[4];
            printf("  Low (1-18) or High (19-36)? (l/h): ");
            fflush(stdout);
            if (scanf("%3s",c)!=1){flush_stdin();continue;}
            flush_stdin();
            char ch=tolower((unsigned char)c[0]);
            if      (ch=='h'){b.value=1;strcpy(b.label,"High 19-36");}
            else if (ch=='l'){b.value=0;strcpy(b.label,"Low 1-18");}
            else ok=0;
        }

        if (!ok) { printf("  " RED "Invalid input, try again." RST "\n"); continue; }

        p->balance -= amount;
        p->bets[p->nbet++] = b;
        printf("  " GREEN "[OK] $%.2f on %-12s" RST
               "  (Remaining: $%.2f)\n", amount, b.label, p->balance);
    }
}

/* ---- Show round results ----------------------------------------------- */
static void show_results(Player *players, int np, int result) {
    printf(BOLD "\n  ============== RESULTS ==============\n" RST);
    for (int i = 0; i < np; i++) {
        Player *p = &players[i];
        if (p->nbet == 0) continue;
        printf("\n  " BOLD "%s:\n" RST, p->name);
        double net = 0.0;
        for (int j = 0; j < p->nbet; j++) {
            Bet *b  = &p->bets[j];
            double d = eval_bet(b, result);
            net += d;
            if (d > 0)
                printf("    " GREEN "WIN   +$%7.2f" RST "  on %-14s\n", d, b->label);
            else
                printf("    " RED   "LOSS   $%7.2f" RST "  on %-14s\n", b->amount, b->label);
        }
        if (net > 0) p->balance += net;
        const char *col = (net>0)?YELLOW:(net<0)?RED:WHITE;
        printf("  %sNet: %+.2f   Balance: $%.2f\n" RST, col, net, p->balance);
        if (p->balance < 0.01 && p->active) {
            printf("  " RED BOLD "  %s is out of money!\n" RST, p->name);
            p->active = 0;
        }
        p->nbet = 0;
    }
    printf(BOLD "\n  =====================================\n" RST);
}

/* ---- main ------------------------------------------------------------- */
int main(void) {
    srand((unsigned)time(NULL));

    printf(BOLD GREEN
           "\n  +========================================+\n"
           "  |     *** TERMINAL ROULETTE ***          |\n"
           "  |   American wheel  |  $100 start        |\n"
           "  +========================================+\n" RST);

    int np;
    printf("\n  How many players? (1-%d): ", MAX_PLAYERS);
    if (scanf("%d",&np)!=1) np=1;
    flush_stdin();
    if (np<1) np=1;
    if (np>MAX_PLAYERS) np=MAX_PLAYERS;

    Player players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));
    for (int i=0;i<np;i++) {
        printf("  Name for player %d: ", i+1);
        fflush(stdout);
        if (!fgets(players[i].name, MAX_NAME, stdin))
            snprintf(players[i].name, MAX_NAME, "Player %d", i+1);
        players[i].name[strcspn(players[i].name,"\n")]='\0';
        if (!players[i].name[0])
            snprintf(players[i].name, MAX_NAME, "Player %d", i+1);
        players[i].balance = STARTING_BAL;
        players[i].active  = 1;
    }

    for (;;) {
        int alive=0;
        for (int i=0;i<np;i++) if (players[i].active) alive++;
        if (!alive) break;

        printf(BOLD YELLOW
               "\n  ============== NEW ROUND ==============\n" RST);

        int anyone_bet=0;
        for (int i=0;i<np;i++) {
            if (!players[i].active) continue;
            collect_bets(&players[i]);
            if (players[i].nbet>0) anyone_bet=1;
        }

        if (!anyone_bet) {
            printf("  No bets placed -- skipping spin.\n");
        } else {
            int result = spin_wheel();
            show_results(players, np, result);
        }

        alive=0;
        for (int i=0;i<np;i++) if (players[i].active) alive++;
        if (!alive) {
            printf(RED BOLD "\n  All players broke. Game over!\n" RST);
            break;
        }

        printf("\n  Spin again? (y/n): ");
        fflush(stdout);
        char ch[4];
        if (scanf("%3s",ch)!=1) break;
        flush_stdin();
        if (tolower((unsigned char)ch[0])!='y') break;
    }

    printf(BOLD CYAN "\n  ========== FINAL BALANCES ==========\n" RST);
    for (int i=0;i<np;i++) {
        double diff = players[i].balance - STARTING_BAL;
        const char *col = (diff>=0)?GREEN:RED;
        printf("  %-20s $%8.2f   %s(%+.2f)" RST "\n",
               players[i].name, players[i].balance, col, diff);
    }
    printf(BOLD CYAN "  =====================================\n" RST
           BOLD "\n  Thanks for playing!\n\n" RST);
    return 0;
}
