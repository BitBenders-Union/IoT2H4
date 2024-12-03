from src.nlu import NLU
from src.nlg import NLG

class ChatBot:
    def __init__(self):
        self.nlu = NLU()
        self.nlu.load('./models/intent_classifier.pkl', './models/vectorizer.pkl')
        self.nlg = NLG('./data/intents.json')

    def chat(self, user_input):
        intent = self.nlu.classify(user_input)
        return self.nlg.generate_response(intent)

if __name__ == "__main__":
    bot = ChatBot()
    while True:
        user_input = input("You: ")
        if user_input.lower() == "exit":
            break
        print(f"Bot: {bot.chat(user_input)}")
