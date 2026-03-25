import random

class BracketGenerator:
    def __init__(self, model):
        self.model = model

    def simulate_game(self, team_a, team_b):
        """Simulate a single game and return the winner."""
        rating_a = self.model.get_rating(team_a)
        rating_b = self.model.get_rating(team_b)
        
        prob_a = self.model.calculate_expected_score(rating_a, rating_b)
        
        # In a deterministic mode, the higher rating always wins.
        # But let's add a bit of randomness for flavor.
        if random.random() < prob_a:
            return team_a
        else:
            return team_b

    def generate_bracket(self, seeds_df):
        """
        Simulate the tournament rounds.
        Expects seeds_df with columns ['region', 'seed', 'team'].
        This is a simplified 16-team simulation for demo purposes.
        """
        results = {}
        
        # Round 1: Let's assume the provided seeds are our 16 teams.
        # 1 vs 4, 2 vs 3 in each region
        regions = seeds_df['region'].unique()
        
        round_1_winners = []
        for region in regions:
            region_teams = seeds_df[seeds_df['region'] == region]
            
            # 1 vs 4
            t1 = region_teams[region_teams['seed'] == 1]['team'].values[0]
            t4 = region_teams[region_teams['seed'] == 4]['team'].values[0]
            winner_1_4 = self.simulate_game(t1, t4)
            round_1_winners.append(winner_1_4)
            
            # 2 vs 3
            t2 = region_teams[region_teams['seed'] == 2]['team'].values[0]
            t3 = region_teams[region_teams['seed'] == 3]['team'].values[0]
            winner_2_3 = self.simulate_game(t2, t3)
            round_1_winners.append(winner_2_3)

        results['Round 1 Winners'] = round_1_winners
        
        # Round 2: Elite Eight (Winners of each region's 1-4 vs 2-3)
        round_2_winners = []
        for i in range(0, len(round_1_winners), 2):
            winner = self.simulate_game(round_1_winners[i], round_1_winners[i+1])
            round_2_winners.append(winner)
        
        results['Elite Eight Winners'] = round_2_winners
        
        # Round 3: Final Four
        finalists = []
        for i in range(0, len(round_2_winners), 2):
            winner = self.simulate_game(round_2_winners[i], round_2_winners[i+1])
            finalists.append(winner)
            
        results['Finalists'] = finalists
        
        # Championship
        champion = self.simulate_game(finalists[0], finalists[1])
        results['Champion'] = champion
        
        return results
