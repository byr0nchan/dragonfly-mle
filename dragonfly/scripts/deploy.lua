-- Deploy contains the single script needed to deploy a model

require("logistic-regression") --contains classify function
require("dt") -- contains data transformation functions

function score(input, model)
	return classify(transform(input), model.weights, model.num_features)
end


model = read_model("cf_domains_1.model")
pred = score("accountingheadninegenius.com", model)
print(pred)