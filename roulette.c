/*
 * +========================================+
 * |      *** TERMINAL ROULETTE ***         |
 * |   American wheel  |  $100 start        |
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

/* ---- Config ----------------------------------------------------------- */
#define MAX_PLAYERS  8
#define MAX_NAME     32
#define MAX_BETS     10
#define STARTING_BAL 100.0

/* ---- ANSI ------------------------------------------------------------- */
#define RST    "\033[0m"
#define BOLD   "\033[1m"
#define RED    "\033[31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define CYAN   "\033[36m"
#define WHITE  "\033[97m"
#define BG_RED "\033[41m"
#define BG_GRN "\033[42m"
#define BG_BLK "\033[40;97m"

/* ---- American roulette wheel order (37 == "00") ----------------------- */
static const int WHEEL[38] = {
     0, 28,  9, 26, 30, 11,  7, 20, 32, 17,
     5, 22, 34, 15,  3, 24, 36, 13,  1, 37,
    27, 10, 25, 29, 12,  8, 19, 31, 18,  6,
    21, 33, 16,  4, 23, 35, 14,  2
};
static const int RED_NUMS[18] = {
    1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36
};

/* ---- Types ------------------------------------------------------------ */
typedef enum { COL_RED, COL_BLACK, COL_GREEN } Color;
typedef enum { BET_NUMBER, BET_COLOR, BET_ODD_EVEN, BET_HIGH_LOW } BetType;

typedef struct {
    BetType type;
    double  amount;
    int     number;   /* BET_NUMBER: 0-36 or 37 for 00 */
    int     value;    /* color/odd-even/high-low selector */
    char    label[24];
} Bet;

typedef struct {
    char   name[MAX_NAME];
    double balance;
    int    active;
    Bet    bets[MAX_BETS];
    int    nbet;
} Player;

/* ---- Helpers ---------------------------------------------------------- */
static void flush_stdin(void) { int c; while ((c=getchar())!='\n'&&c!=EOF); }

static Color num_color(int n) {
    if (n==0||n==37) return COL_GREEN;
    for (int i=0;i<18;i++) if (RED_NUMS[i]==n) return COL_RED;
    return COL_BLACK;
}

/* Print one pocket cell — always exactly 4 visible characters " NN " */
static void pcell(int n) {
    Color c = num_color(n);
    const char *bg = (c==COL_RED) ? BG_RED : (c==COL_GREEN) ? BG_GRN : BG_BLK;
    if (n==37) printf("%s%s 00 %s", BOLD, bg, RST);
    else       printf("%s%s %2d %s", BOLD, bg, n, RST);
}

/* ---- Spin animation --------------------------------------------------- */
/*
 * Strategy: print a static top border, then spin the pocket row in-place
 * using \r (carriage return) — no cursor-up needed, no line-count issues.
 * Finally print a static bottom border and the result.
 *
 * Visual (each frame):
 *
 *   +-----  W H E E L  -----+  (printed once before loop)
 *   |  xx  xx  xx [YY] xx  xx  xx  |   <- \r redraws this line each frame
 *   +------------------------+  (printed once after loop)
 */
static int spin_wheel(void) {
    int landing = rand() % 38;

    /* Delays in ms — starts fast, decelerates */
    static const int DELAYS[] = {
        35, 37, 40, 44, 49, 55, 62, 71, 81, 93,
        107,123,142,164,189,218,251,289,332,380,433
    };
    int nd = (int)(sizeof(DELAYS)/sizeof(DELAYS[0]));
    int frames = 24;   /* total animation frames */

    /* Each visible pocket = 4 chars.  We show 9 pockets plus borders:
     *   "| " + 4*side + " [" + 4*centre + "] " + 4*side*4 + " |"
     * Let's show 4 pockets either side of centre = 9 total.
     * Line width: 2 + 4*4 + 3 + 4 + 3 + 4*4 + 2 = 2+16+3+4+3+16+2 = 46 visible chars — fits 80 cols.
     */

    printf("\n  " BOLD "*** Spinning the wheel... ***\n\n" RST);

    /* Static top border — printed once */
    printf("  +--------------------------------------------------+\n");

    /* Spin loop — all on one line using \\r */
    for (int f = 0; f < frames; f++) {
        /* Walk index toward landing as wheel decelerates */
        int pos = ((landing - (frames - f)) % 38 + 38*100) % 38;

        printf("\r  |  ");
        /* 4 pockets to the left */
        for (int i = -4; i < 0; i++) {
            int idx = ((pos+i)%38+38)%38;
            pcell(WHEEL[idx]);
            printf(" ");
        }
        /* centre pocket — highlighted */
        printf(BOLD WHITE ">" RST);
        pcell(WHEEL[pos]);
        printf(BOLD WHITE "<" RST);
        /* 4 pockets to the right */
        for (int i = 1; i <= 4; i++) {
            int idx = ((pos+i)%38+38)%38;
            printf(" ");
            pcell(WHEEL[idx]);
        }
        printf("  |  ");
        fflush(stdout);

        int delay = (f < nd) ? DELAYS[f] : DELAYS[nd-1] + (f-nd+1)*52;
        usleep((unsigned)(delay * 1000));
    }

    /* Static bottom border — printed once after loop */
    printf("\n  +--------------------------------------------------+\n");

    /* Result */
    int result = WHEEL[landing];
    Color rc   = num_color(result);
    const char *cname =
        (rc==COL_RED)   ? RED   "RED"   RST :
        (rc==COL_GREEN) ? GREEN "GREEN" RST :
                          WHITE "BLACK" RST;

    printf("\n  " BOLD "  Ball lands on: ");
    pcell(result);
    printf("  ( %s" BOLD " )\n\n" RST, cname);
    return result;
}

/* ---- Evaluate a bet --------------------------------------------------- */
static double eval_bet(const Bet *b, int result) {
    Color rc = num_color(result);
    switch (b->type) {
    case BET_NUMBER:
        return (b->number==result) ? b->amount*35.0 : -b->amount;
    case BET_COLOR:
        if (rc==COL_GREEN) return -b->amount;
        return ((b->value==0&&rc==COL_RED)||(b->value==1&&rc==COL_BLACK))
               ? b->amount : -b->amount;
    case BET_ODD_EVEN:
        if (result==0||result==37) return -b->amount;
        return ((b->value==1&&result%2==1)||(b->value==0&&result%2==0))
               ? b->amount : -b->amount;
    case BET_HIGH_LOW:
        if (result<1||result>36) return -b->amount;
        return ((b->value==1&&result>=19)||(b->value==0&&result<=18))
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
               "    1) Number 0-36 / 00      pays 35:1\n"
               "    2) Red / Black           pays  1:1\n"
               "    3) Odd / Even            pays  1:1\n"
               "    4) Low 1-18 / High 19-36 pays  1:1\n"
               "    0) Done\n"
               "  > ");
        fflush(stdout);

        int choice;
        if (scanf("%d",&choice)!=1){flush_stdin();continue;}
        flush_stdin();
        if (choice==0) break;
        if (choice<1||choice>4){printf("  Invalid.\n");continue;}

        double amount;
        printf("  Amount ($0.01 - $%.2f): $", p->balance);
        fflush(stdout);
        if (scanf("%lf",&amount)!=1){flush_stdin();continue;}
        flush_stdin();
        if (amount<=0.0||amount>p->balance){
            printf("  " RED "Invalid amount." RST "\n"); continue;
        }

        Bet b; memset(&b,0,sizeof(b));
        b.amount = amount;
        int ok = 1;

        if (choice==1) {
            b.type = BET_NUMBER;
            char buf[8];
            printf("  Number (0-36, or 00): ");
            fflush(stdout);
            if (scanf("%7s",buf)!=1){flush_stdin();continue;}
            flush_stdin();
            if (strcmp(buf,"00")==0) {
                b.number=37; snprintf(b.label,sizeof(b.label),"00");
            } else {
                for (int i=0;buf[i];i++) if(!isdigit((unsigned char)buf[i])){ok=0;break;}
                int n=atoi(buf);
                if (n<0||n>36) ok=0;
                if (ok){b.number=n;snprintf(b.label,sizeof(b.label),"%d",n);}
            }
        } else if (choice==2) {
            b.type=BET_COLOR;
            char c[4];
            printf("  Red or Black? (r/b): ");
            fflush(stdout);
            if (scanf("%3s",c)!=1){flush_stdin();continue;}
            flush_stdin();
            char ch=tolower((unsigned char)c[0]);
            if      (ch=='r'){b.value=0;strcpy(b.label,"Red");}
            else if (ch=='b'){b.value=1;strcpy(b.label,"Black");}
            else ok=0;
        } else if (choice==3) {
            b.type=BET_ODD_EVEN;
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
            b.type=BET_HIGH_LOW;
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

        if (!ok){printf("  " RED "Invalid input." RST "\n");continue;}

        p->balance -= amount;
        p->bets[p->nbet++] = b;
        printf("  " GREEN "[OK] $%.2f on %-12s" RST
               "  (Remaining: $%.2f)\n", amount, b.label, p->balance);
    }
}

/* ---- Show round results ----------------------------------------------- */
static void show_results(Player *players, int np, int result) {
    printf(BOLD "\n  ============== RESULTS ==============\n" RST);
    for (int i=0;i<np;i++) {
        Player *p = &players[i];
        if (p->nbet==0) continue;
        printf("\n  " BOLD "%s:\n" RST, p->name);
        double net = 0.0;
        for (int j=0;j<p->nbet;j++) {
            Bet *b = &p->bets[j];
            double d = eval_bet(b, result);
            net += d;
            if (d>0)
                printf("    " GREEN "WIN   +$%7.2f" RST "  on %-14s\n", d, b->label);
            else
                printf("    " RED   "LOSS   $%7.2f" RST "  on %-14s\n", b->amount, b->label);
        }
        if (net>0) p->balance += net;
        const char *col=(net>0)?YELLOW:(net<0)?RED:WHITE;
        printf("  %sNet: %+.2f   Balance: $%.2f\n" RST, col, net, p->balance);
        if (p->balance<0.01&&p->active) {
            printf("  " RED BOLD "  %s is out of money!\n" RST, p->name);
            p->active=0;
        }
        p->nbet=0;
    }
    printf(BOLD "\n  =====================================\n" RST);
}

/* ---- main ------------------------------------------------------------- */
int main(void) {
    srand((unsigned)time(NULL));

    printf(BOLD GREEN
           "\n  +========================================+\n"
           "  |      *** TERMINAL ROULETTE ***         |\n"
           "  |   American wheel  |  $100 start        |\n"
           "  +========================================+\n" RST);

    int np;
    printf("\n  How many players? (1-%d): ", MAX_PLAYERS);
    if (scanf("%d",&np)!=1) np=1;
    flush_stdin();
    if (np<1) np=1;
    if (np>MAX_PLAYERS) np=MAX_PLAYERS;

    Player players[MAX_PLAYERS];
    memset(players,0,sizeof(players));
    for (int i=0;i<np;i++) {
        printf("  Name for player %d: ", i+1);
        fflush(stdout);
        if (!fgets(players[i].name,MAX_NAME,stdin))
            snprintf(players[i].name,MAX_NAME,"Player %d",i+1);
        players[i].name[strcspn(players[i].name,"\n")]='\0';
        if (!players[i].name[0])
            snprintf(players[i].name,MAX_NAME,"Player %d",i+1);
        players[i].balance=STARTING_BAL;
        players[i].active=1;
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
        double diff=players[i].balance-STARTING_BAL;
        const char *col=(diff>=0)?GREEN:RED;
        printf("  %-20s $%8.2f   %s(%+.2f)" RST "\n",
               players[i].name, players[i].balance, col, diff);
    }
    printf(BOLD CYAN "  =====================================\n" RST
           BOLD "\n  Thanks for playing!\n\n" RST);
    return 0;
}
