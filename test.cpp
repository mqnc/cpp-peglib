
#include <assert.h>
#include <iostream>
#include <string>
#include "peglib.h"

using namespace peg;
using namespace std;


struct Explorer : public Ope::Visitor {
	size_t depth;
	Explorer(size_t depth) :depth{ depth } {}

	void visit(Sequence& ope) override {
		result += "(";
		for (auto op : ope.opes_) {
			op->accept(*this);
			result += " ";
		}
		result += ")";
	}
	void visit(PrioritizedChoice& ope) override {
		result += "(";
		for (auto op : ope.opes_) {
			op->accept(*this);
			result += " / ";
		}
		result += ")";
	}
	void visit(Repetition& ope) override {
		ope.ope_->accept(*this);
		result += "{" + std::to_string(ope.min_) + "," + std::to_string(ope.min_) + "}";
	}
	void visit(AndPredicate& ope) override {
		result += "&";
		ope.ope_->accept(*this);
	}
	void visit(NotPredicate& ope) override {
		if (depth <= 0) { result += "..."; return; }
		result += "!";
		depth--;
		ope.ope_->accept(*this);
		depth++;
	}


	void visit(LiteralString& ope) override { result += ope.lit_; }
	void visit(AnyCharacter&) override { result += "."; }

	void visit(Ignore& ope) override {
		result += "~";
		ope.ope_->accept(*this);
	}

	void visit(WeakHolder& ope) override {
		if (depth <= 0) { result += "wh(...)"; return; }
		result += "wh(";
		depth--;
		ope.weak_.lock()->accept(*this);
		depth++;
		result += ")";
	}
	void visit(Holder& ope) override {
		if (depth <= 0) { result += "h(...)"; return; }
		result += "h(";
		depth--;
		result += ope.outer_->name;
		ope.ope_->accept(*this);
		depth++;
		result += ")";
	}
	void visit(Reference& ope) override {
		if (depth <= 0) { result += "&(...)"; return; }
		result += "&(";
		depth--;
		result += ope.name_;
		ope.rule_->accept(*this);
		depth++;
		result += ")";
	};

	std::string result;
};



struct DetectLeftRecursion2 : public Ope::Visitor {
	DetectLeftRecursion2(const std::string& name) : name_(name) {}

	void visit(Sequence& ope) override {
		for (auto op : ope.opes_) {
			op->accept(*this);
			if (done_) {
				break;
			}
			else if (error_s) {
				done_ = true;
				break;
			}
		}
	}
	void visit(PrioritizedChoice& ope) override {
		for (auto op : ope.opes_) {
			op->accept(*this);
			if (error_s) {
				done_ = true;
				break;
			}
		}
	}
	void visit(Repetition& ope) override {
		ope.ope_->accept(*this);
		done_ = ope.min_ > 0;
	}
	void visit(AndPredicate& ope) override {
		ope.ope_->accept(*this);
		done_ = false;
	}
	void visit(NotPredicate& ope) override {
		ope.ope_->accept(*this);
		done_ = false;
	}
	void visit(Dictionary&) override { done_ = true; }
	void visit(LiteralString& ope) override { done_ = !ope.lit_.empty(); }
	void visit(CharacterClass&) override { done_ = true; }
	void visit(Character&) override { done_ = true; }
	void visit(AnyCharacter&) override { done_ = true; }
	void visit(CaptureScope& ope) override { ope.ope_->accept(*this); }
	void visit(Capture& ope) override { ope.ope_->accept(*this); }
	void visit(TokenBoundary& ope) override { ope.ope_->accept(*this); }
	void visit(Ignore& ope) override { ope.ope_->accept(*this); }
	void visit(User&) override { done_ = true; }
	void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
	void visit(Holder& ope) override {
		auto& name = ope.outer_->name;
		if (!visitedHolders_.count(name)) {
			visitedHolders_.insert(name);
			ope.outer_->accept(*this);
			if (done_ == false) { return; }
		}
		else {
			if (name == name_) {
				error_s = "(anonymous)";
			}
		}
		done_ = true;
	}
	void visit(Reference& ope) override { ope.rule_->accept(*this); }
	void visit(Whitespace& ope) override { ope.ope_->accept(*this); }
	void visit(BackReference&) override { done_ = true; }
	void visit(PrecedenceClimbing& ope) override { ope.atom_->accept(*this); }
	void visit(Recovery& ope) override { ope.ope_->accept(*this); }
	void visit(Cut&) override { done_ = true; }

	const char* error_s = nullptr;

private:
	std::string name_;
	std::set<std::string> visitedHolders_;
	bool done_ = false;
};

int main(void) {
	{
		Definition ROOT, A, B, X;
		ROOT <= oom(cho(A, B, X));
		A <= chr('1');
		A = [&](const SemanticValues& vs) { std::cout << "a"; };
		B <= chr('2');
		B = [&](const SemanticValues& vs) { std::cout << "b"; };
		X <= chr('x');
		X = [&](const SemanticValues& vs) {
			std::cout << "x";
			A <= chr('2');
			A = [&](const SemanticValues& vs) { std::cout << "A"; };
			B <= chr('1');
			B = [&](const SemanticValues& vs) { std::cout << "B"; };
			X <= chr('X');
			X = [&](const SemanticValues& vs) { std::cout << "X"; };
		};

		auto ret = ROOT.parse("12x12X");
	}

	// using Rules = std::unordered_map<std::string, std::shared_ptr<Ope>>;
	// using Grammar = std::unordered_map<std::string, Definition>;
	std::cout << "\n";
	{
		Definition A, B;
		auto s = seq(A, B);
		A <= s;
		A.name = "A";
		B <= seq(A, B);
		B.name = "B";

		DetectLeftRecursion vis("A");
		A.accept(vis);
		if (vis.error_s) {
			std::cout << vis.error_s << " is left recursive" << "\n";
		}
		else {
			std::cout << "ok" << "\n";
		}

		std::cout << TraceOpeName::get(*(shared_ptr<Ope>)A) << "\n";
		//std::cout << TraceOpeName::get(*A.holder_) << "\n";

		Explorer ex(2);
		s->accept(ex);
		std::cout << ex.result << "\n";
	}
	/*{
		Grammar grammar;
		grammar["A"] <= seq(ref(grammar, "A",);
		grammar["A"].name = "A";
		grammar["B"] <= seq(A, B);
		grammar["B"].name = "B";
	}*/
	{
		auto grammar = R"(
		# Grammar for Calculator...
		Additive    <- Multitive '+' Additive / Multitive
		Multitive   <- Primary '*' Multitive / Primary
		Primary     <- '(' Additive ')' / Number
		Number      <- Number < [0-9]+ >
		%whitespace <- [ \t]*
		)";

		parser parser;


		parser.log = [](size_t line, size_t col, const string& msg) {
			cerr << line << ":" << col << ": " << msg << "\n";
		};

		auto ok = parser.load_grammar(grammar);
		if (ok) {
			Explorer ex(2);
			auto def = parser["Additive"];
			def.accept(ex);
			std::cout << ex.result << "\n";
		}
		assert(ok);
	}
}