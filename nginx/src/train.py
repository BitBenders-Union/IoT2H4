import json
from nlu import NLU

def load_data(data_path):
    with open(data_path, 'r') as f:
        intents = json.load(f)
    data = []
    for intent in intents['intents']:
        for pattern in intent['patterns']:
            data.append({'text': pattern, 'intent': intent['tag']})
    return data

if __name__ == "__main__":
    nlu = NLU()
    data = load_data("./data/intents.json")
    nlu.train(data)
    nlu.save('./models/intent_classifier.pkl', './models/vectorizer.pkl')
