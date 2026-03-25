class EloModel:
    def __init__(self, k_factor=20, default_rating=1500):
        self.k_factor = k_factor
        self.default_rating = default_rating
        self.ratings = {}

    def get_rating(self, team_id):
        return self.ratings.get(team_id, self.default_rating)

    def calculate_expected_score(self, rating_a, rating_b):
        return 1 / (1 + 10 ** ((rating_b - rating_a) / 400))

    def update_ratings(self, team_a, team_b, score_a, score_b):
        rating_a = self.get_rating(team_a)
        rating_b = self.get_rating(team_b)

        expected_a = self.calculate_expected_score(rating_a, rating_b)
        expected_b = self.calculate_expected_score(rating_b, rating_a)

        # Actual score: 1 for win, 0.5 for tie, 0 for loss
        if score_a > score_b:
            actual_a, actual_b = 1, 0
        elif score_a < score_b:
            actual_a, actual_b = 0, 1
        else:
            actual_a, actual_b = 0.5, 0.5

        new_rating_a = rating_a + self.k_factor * (actual_a - expected_a)
        new_rating_b = rating_b + self.k_factor * (actual_b - expected_b)

        self.ratings[team_a] = new_rating_a
        self.ratings[team_b] = new_rating_b

        return new_rating_a, new_rating_b

    def fit(self, games_df):
        """
        games_df should have columns: 'team_a', 'team_b', 'score_a', 'score_b'
        sorted by date.
        """
        for _, row in games_df.iterrows():
            self.update_ratings(row['team_a'], row['team_b'], row['score_a'], row['score_b'])
