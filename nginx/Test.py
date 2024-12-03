from src.nlu import NLU
from src.nlg import NLG

nlu = NLU()
nlu.load('./models/intent_classifier.pkl', './models/vectorizer.pkl')

# Test the NLU class
test_input = "Hello"
print(nlu.classify(test_input))  # Check the output of classification
