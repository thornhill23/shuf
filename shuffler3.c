#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define NDECKS		6
#define NGROUPS		38 //36,40?
#define GROUPMAX	10
#define SHOEMIN		6
#define DEALMAX		NDECKS*52 // shoe does not need to be this large
#define NDEALS 		52*NDECKS*10000
#define SPACINGMAX  52*NDECKS*20
//#define SPACINGMAX  2048
#define HISTSIZE	5 * SPACINGMAX / (52*NDECKS)
#define FIFO_GROUPS 1

typedef struct {
	int feltval[3];
	int grpcnt[NGROUPS];
	int groups[NGROUPS][GROUPMAX];
	int shoe[DEALMAX];
} Shuffler;

int bad_group_counts(Shuffler *s) { 
	for (int i = 0; i < NGROUPS; i++) {
		if (s->grpcnt[i] < 0) {
			return 1;
		}
	}
	return 0;
}

void feed_shoe(Shuffler *s, int *card) {
	int n_fullgroups = 0;

	for (int i = 0; i < NGROUPS; i++)
		if (s->grpcnt[i] > 0) n_fullgroups++;

	int f;
	int rnd = 0;
	rnd = rand() % n_fullgroups;
	f = 0;
	for (int g = 0; g < NGROUPS; g++) {
		if (s->grpcnt[g] > 0) {
			if (f++ == rnd) {
				if (FIFO_GROUPS) {
					for (int j = 0; j < s->grpcnt[g]; j++) {
						s->shoe[*card+j] = s->groups[g][j];
						s->groups[g][j] = -1;
					}
				} else {
					for (int j = 0; j < s->grpcnt[g]; j++) {
						s->shoe[*card+j] = s->groups[g][s->grpcnt[g]-j-1];
						s->groups[g][s->grpcnt[g]-j-1] = -1;
					}
				}
				s->grpcnt[g] = 0;
				return;
			}
		}
	}
}

int shift_shoe_forward(Shuffler *s, int n) {
	int i = 0;
	while((n+i < DEALMAX) && (s->shoe[n+i] > -1)) {
		s->shoe[i] = s->shoe[n+i];
		s->shoe[n+i] = -1;
		++i;
	}
	return i;
}

void shuffle_in(Shuffler *s, int *cards, int ncards) { 
	int n_opengroups = 0;
	for (int i = 0; i < NGROUPS; i++)
		if (s->grpcnt[i] < GROUPMAX) n_opengroups++;

	int f,rnd;
//	for (int i = ncards-1; i > -1; i--) { 
	for (int i = 0; i < ncards; i++) { 
		rnd = rand() % n_opengroups;
		f = 0;
		for (int g = 0; g < NGROUPS; g++) {
			if (s->grpcnt[g] < GROUPMAX) {
				if (f++ == rnd) {
					s->groups[g][(s->grpcnt[g])++] = cards[i];
					cards[i] = -1;
					if (s->grpcnt[g] == GROUPMAX) n_opengroups--;
					break;
				}
			}
		}
	}
	int cards_remaining = 0; 
	if (cards == s->shoe)
		cards_remaining = shift_shoe_forward(s,ncards);
	feed_shoe(s,&cards_remaining);
}

void load_shuffler(Shuffler *s) {
	for (int i = 0; i < DEALMAX; i++)
		s->shoe[i] = -1;
	for (int i = 0; i < 3; i++)
		s->feltval[i] = -1;
	for (int i = 0; i < NGROUPS; i++)
		s->grpcnt[i] = 0;
	for (int i = 0; i < NGROUPS; i++)
		for (int j = 0; j < GROUPMAX; j++)
			s->groups[i][j] = -1;

	int n = 52*NDECKS;
	int decks[n]; 
	for (int i = 0; i < n; i++)
		decks[i] = i;
	int t,j;
    for (int i = 0; i < n - 1; i++) {
      j = i + rand() / (RAND_MAX / (n - i) + 1);
      t = decks[j];
      decks[j] = decks[i];
      decks[i] = t;
    }
	shuffle_in(s,decks,n);
}

int card_value(int u) { 
	if (u == -1) return u;
	int x = (u % 52) / 4 + 2;
	if (x == 14) {
		return 11;
	} else if (x > 10) { 
		return 10;
	}
	return x;
}

void hit(int *card, Shuffler *s, int *player) { 
	int v = card_value(s->shoe[*card]);
	if (s->shoe[*card+SHOEMIN] == -1) { 
		feed_shoe(s,card);
		v = card_value(s->shoe[*card]);
	}
	if (v < 11) { 
		player[0] += v;
		player[1] += v;
	} else if (player[0] != player[1]) {
		player[0] += 1;
		player[1] += 1;
	} else {
		player[0] += 11;
		player[1] +=  1;
	}
	(*card)++;
}

void record_spacing_freq_D(int card, int spacing[][HISTSIZE], int *nspaces, long *spacing_hist) {
	int runsum = 0;
	for (int i  = nspaces[card] -1; i > -1; i--)
		runsum += spacing[card][i];

	if (runsum >= SPACINGMAX) {
		spacing[card][0] = spacing[card][nspaces[card]-1];
		spacing_hist[ spacing[card][0] ]++;
		for (int i = 1; i < nspaces[card]; i++)
			spacing[card][i] = 0;
		nspaces[card] = 1;
	} else {
		spacing_hist[runsum]++;
	}
	return;
}

void record_spacing_freq(int card, int spacing[][HISTSIZE], int *nspaces, long *spacing_hist) {
	int trim = 0;
	int runsum = 0;
	for (int i  = nspaces[card] -1; i > -1; i--) {
		runsum += spacing[card][i];
		if (runsum >= SPACINGMAX) {
			trim = i + 1;
			break;
		}
		spacing_hist[runsum]++;
	}

	if (trim) {
		for (int i = 0; i < nspaces[card] - trim; i++)
			spacing[card][i] = spacing[card][i + trim];
		for (int i = nspaces[card] - trim; i < nspaces[card]; i++)
			spacing[card][i] = 0;
		nspaces[card] -= trim;
	}
	return;
}

void record_spacing(int card, int spacing[][HISTSIZE], int *nspaces, long *spacing_hist) {
	for (int c = 0; c < 52*NDECKS; c++)
		if ((spacing[c][0] >= 0) || (c == card))
			spacing[c][nspaces[c]]++;
	if (spacing[card][nspaces[card]] > 0) {
		nspaces[card]++;
	//	record_spacing_freq_D(card,spacing, nspaces, spacing_hist);
		record_spacing_freq(card,spacing, nspaces, spacing_hist);
	}
	return;
}

void deal(Shuffler *s, int spacing[][HISTSIZE], int *nspaces, long *spacing_hist) {
	int *f;
	f = s->feltval;

	int card_count = 0;
	int *c = &card_count;
	int d[2] = {0};
	int p[2] = {0};
	hit(c,s,p);
	hit(c,s,d);
	hit(c,s,p);
	for (int i=0; i < 3; i++)
		f[i] = card_value(s->shoe[i]);
	if (f[0] + f[2] == 21) {
		if (f[1] < 10) {
			(void)0;
		} else {
			hit(c,s,d);
		}
	} else if ((f[0] == f[2]) && !((f[0] == 10) || (f[0] == 5))) {
		int w[2] = {p[0],p[1]}; // split
		if (f[0] == 11) {
			hit(c,s,p);
			hit(c,s,w);
		} else if (f[1] < 7) {
			while ((p[0] < 12) && (p[1] < 18)) hit(c,s,p);
			while ((w[0] < 12) && (w[1] < 18)) hit(c,s,w);
		} else {
			while ((p[0] < 17) && (p[1] < 19)) hit(c,s,p);
			while ((w[0] < 17) && (w[1] < 19)) hit(c,s,w);
		}
	} else if (f[1] >= 7) {
		while ((p[0] < 17) && (p[1] < 19)) hit(c,s,p);
	} else { //f[1] <= 7
		while ((p[0] < 12) && (p[1] < 18)) hit(c,s,p);
	}

	if (f[0] + f[2] != 21)
		while ((d[0] < 17) && (d[1] < 18)) hit(c,s,d);

	record_spacing(s->shoe[*c], spacing, nspaces, spacing_hist);

	// cards dealt 123456... go back in 312456...
	for (int i = 0; i < 3; i++)
		s->feltval[i] = s->shoe[i];
	s->shoe[0] = s->feltval[2];
	s->shoe[1] = s->feltval[0];
	s->shoe[2] = s->feltval[1];
	shuffle_in(s,s->shoe,*c);
	return;
}

int main() {
	srand(time(NULL));
	int spacing[52*NDECKS][HISTSIZE] = {{0}};
	for (int i = 0; i < 52*NDECKS; i++)
		spacing[i][0] = -1;
	int nspaces[52*NDECKS] = {0};
	long spacing_hist[SPACINGMAX] = {0};

	Shuffler s;
	load_shuffler(&s);

	for (int i = 0; i < NDEALS; i++)
		deal(&s, spacing, nspaces, spacing_hist);

	FILE *file_sh = fopen("spacing_hist.csv","a");
//	FILE *file_sp = fopen("spacing_prob.csv","a");
	for (int j = 0; j < SPACINGMAX; j++) //{
		fprintf(file_sh,"%ld,",spacing_hist[j]);
//	fprintf(file_sh,"\n");
	fclose(file_sh);
	return 0;
}
