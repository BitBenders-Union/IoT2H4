import random
import json

class NLG:
    def __init__(self, responses_path):
        with open(responses_path, 'r') as f:
            self.responses = json.load(f)

    def generate_response(self, intent):
        for intent_data in self.responses['intents']:
            if intent_data['tag'] == intent:
                return random.choice(intent_data['responses'])
        return "Sorry, I didn't understand that."
