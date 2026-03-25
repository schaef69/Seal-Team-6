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
        # TODO: Implement ELO update logic based on game results
        pass
