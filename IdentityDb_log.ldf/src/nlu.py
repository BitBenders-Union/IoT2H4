import json
from sklearn.feature_extraction.text import CountVectorizer
from sklearn.naive_bayes import MultinomialNB
import pickle

class NLU:
    def __init__(self):
        self.vectorizer = CountVectorizer()
        self.model = MultinomialNB()

    def train(self, data):
        texts, intents = zip(*[(d['text'], d['intent']) for d in data])
        X = self.vectorizer.fit_transform(texts)
        self.model.fit(X, intents)

    def classify(self, text):
        X = self.vectorizer.transform([text])
        return self.model.predict(X)[0]

    def save(self, model_path, vectorizer_path):
        with open(model_path, 'wb') as f:
            pickle.dump(self.model, f)
        with open(vectorizer_path, 'wb') as f:
            pickle.dump(self.vectorizer, f)

    def load(self, model_path, vectorizer_path):
        with open(model_path, 'rb') as f:
            self.model = pickle.load(f)
        with open(vectorizer_path, 'rb') as f:
            self.vectorizer = pickle.load(f)
