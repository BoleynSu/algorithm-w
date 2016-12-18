#ifndef SU_BOLEYN_BSL_PARSE_H
#define SU_BOLEYN_BSL_PARSE_H

#include <cassert>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "data.h"
#include "expr.h"
#include "lex.h"
#include "sig_check.h"
#include "type.h"

using namespace std;

struct Parser {
  Lexer &lexer;
  Token t;
  shared_ptr<map<string, shared_ptr<Data>>> data_decl;
  shared_ptr<map<string, shared_ptr<Constructor>>> constructor_decl;

  Parser(Lexer &lexer) : lexer(lexer) {}

  bool match(TokenType token_type) {
    while (lexer.look_at(0).token_type == TokenType::HASHBANG ||
           lexer.look_at(0).token_type == TokenType::SPACE ||
           lexer.look_at(0).token_type == TokenType::COMMENT) {
      lexer.next();
    }
    t = lexer.look_at(0);
    return lexer.look_at(0).token_type == token_type;
  }

  bool accept(TokenType token_type) {
    if (match(token_type)) {
      lexer.next();
      return true;
    }
    return false;
  }

  void expect(TokenType token_type) {
    if (!accept(token_type)) {
      string data = lexer.look_at(0).data;
      if (data.length() > 78) {
        data = data.substr(0, 75) + "...";
      }
      cerr << "parser: " << lexer.look_at(0).position << " "
           << "expect " << token_type << " but get "
           << lexer.look_at(0).token_type << endl
           << "`" << data << "`" << endl;
      exit(EXIT_FAILURE);
    }
  }

  shared_ptr<Poly_> parse_sig(map<string, shared_ptr<Mono_>> &m) {
    auto mo = parse_sig_(m);
    if (accept(TokenType::RIGHTARROW)) {
      auto t = new_const_var("->");
      t->tau.push_back(mo);
      t->tau.push_back(parse_sig(m));
      mo = new_poly(t);
    }
    return mo;
  }
  shared_ptr<Poly_> parse_sig_(map<string, shared_ptr<Mono_>> &m) {
    shared_ptr<Poly_> mo;
    if (accept(TokenType::FORALL)) {
      expect(TokenType::IDENTIFIER);
      if (m.count(t.data)) {
        string data = t.data;
        if (data.length() > 78) {
          data = data.substr(0, 75) + "...";
        }
        cerr << "parser: " << t.position << " type variable names conflict"
             << endl
             << "`" << data << "`" << endl;
        exit(EXIT_FAILURE);
      }
      auto alpha = new_forall_var();
      m[t.data] = alpha;
      expect(TokenType::DOT);
      mo = new_poly(alpha, parse_sig(m));
    } else if (accept(TokenType::IDENTIFIER)) {
      if (m.count(t.data)) {
        mo = new_poly(m[t.data]);
      } else {
        mo = new_poly(new_const_var(t.data));
      }
    } else {
      expect(TokenType::LEFT_PARENTHESIS);
      mo = parse_sig(m);
      expect(TokenType::RIGHT_PARENTHESIS);
    }

    if (mo->is_mono && is_c(get_mono(mo)) && get_mono(mo)->D != "->") {
      auto t1 = get_mono(mo);
      while (match(TokenType::IDENTIFIER) ||
             match(TokenType::LEFT_PARENTHESIS)) {
        if (accept(TokenType::IDENTIFIER)) {
          if (m.count(t.data)) {
            mo = new_poly(m[t.data]);
          } else {
            mo = new_poly(new_const_var(t.data));
          }
        } else {
          expect(TokenType::LEFT_PARENTHESIS);
          mo = parse_sig(m);
          expect(TokenType::RIGHT_PARENTHESIS);
        }
        t1->tau.push_back(mo);
      }
      mo = new_poly(t1);
    }
    return mo;
  }

  shared_ptr<Constructor> parse_constructor(map<string, shared_ptr<Mono_>> &m) {
    auto c = make_shared<Constructor>();
    expect(TokenType::IDENTIFIER);
    if (constructor_decl->count(t.data)) {
      string data = t.data;
      if (data.length() > 78) {
        data = data.substr(0, 75) + "...";
      }
      cerr << "parser: " << t.position << " constructor names conflict" << endl
           << "`" << data << "`" << endl;
      exit(EXIT_FAILURE);
    }
    c->name = t.data;
    expect(TokenType::COLON);
    c->type = parse_sig(m);
    (*constructor_decl)[c->name] = c;
    auto tm = get_mono(c->type);
    c->arg = 0;
    while (is_c(tm) && tm->D == "->") {
      c->arg++;
      tm = get_mono(tm->tau[1]);
    }
    return c;
  }

  shared_ptr<Data> parse_data() {
    auto d = make_shared<Data>();
    expect(TokenType::IDENTIFIER);
    if (data_decl->count(t.data)) {
      string data = t.data;
      if (data.length() > 78) {
        data = data.substr(0, 75) + "...";
      }
      cerr << "parser: " << t.position << " type names conflict" << endl
           << "`" << data << "`" << endl;
      exit(EXIT_FAILURE);
    }
    d->name = t.data;
    d->arg = 0;
    set<string> st;
    while (!match(TokenType::LEFT_BRACE)) {
      expect(TokenType::IDENTIFIER);
      if (st.count(t.data)) {
        string data = t.data;
        if (data.length() > 78) {
          data = data.substr(0, 75) + "...";
        }
        cerr << "parser: " << t.position << " type variable names conflict"
             << endl
             << "`" << data << "`" << endl;
        exit(EXIT_FAILURE);
      }
      st.insert(t.data);
      d->arg++;
    }
    expect(TokenType::LEFT_BRACE);
    while (!accept(TokenType::RIGHT_BRACE)) {
      map<string, shared_ptr<Mono_>> m;
      d->constructors.push_back(parse_constructor(m));
      d->constructors.back()->data_name = d->name;
      if (!match(TokenType::RIGHT_BRACE)) {
        expect(TokenType::SEMICOLON);
      }
    }
    (*data_decl)[d->name] = d;
    return d;
  }

  shared_ptr<Expr> parse_expr() {
    auto expr = make_shared<Expr>();
    if (accept(TokenType::LAMBDA)) {
      expr->T = ExprType::ABS;
      expect(TokenType::IDENTIFIER);
      expr->x = t.data;
      expect(TokenType::RIGHTARROW);
      expr->e = parse_expr();
    } else if (accept(TokenType::LET)) {
      shared_ptr<Poly_> s;
      expr->T = ExprType::LET;
      expect(TokenType::IDENTIFIER);
      expr->x = t.data;
      if (accept(TokenType::COLON)) {
        map<string, shared_ptr<Mono_>> m;
        s = parse_sig(m);
      }
      expect(TokenType::EQUAL);
      expr->e1 = parse_expr();
      expr->e1->sig = s;
      expect(TokenType::IN);
      expr->e2 = parse_expr();
      if (s != nullptr) {
        check(data_decl, expr);
      }
    } else if (accept(TokenType::REC)) {
      expr->T = ExprType::REC;
      set<string> st;
      do {
        shared_ptr<Poly_> s;
        expect(TokenType::IDENTIFIER);
        if (st.count(t.data)) {
          string data = t.data;
          if (data.length() > 78) {
            data = data.substr(0, 75) + "...";
          }
          cerr << "parser: " << t.position << " variable names conflict" << endl
               << "`" << data << "`" << endl;
          exit(EXIT_FAILURE);
        }
        st.insert(t.data);
        expr->xes.push_back(make_pair(t.data, nullptr));
        if (accept(TokenType::COLON)) {
          map<string, shared_ptr<Mono_>> m;
          s = parse_sig(m);
        }
        expect(TokenType::EQUAL);
        expr->xes.back().second = parse_expr();
        expr->xes.back().second->sig = s;
        if (s != nullptr) {
          check(data_decl, expr->xes.back().second);
        }
      } while (accept(TokenType::AND));
      expect(TokenType::IN);
      expr->e = parse_expr();
    } else {
      expr = parse_expr_();
      if (match(TokenType::LAMBDA) || match(TokenType::LET) ||
          match(TokenType::REC)) {
        auto e1 = parse_expr();
        auto e2 = make_shared<Expr>();
        e2->T = ExprType::APP;
        e2->e1 = expr;
        e2->e2 = e1;
        expr = e2;
      }
    }
    return expr;
  }

  shared_ptr<Expr> parse_expr_() {
    auto expr = parse_expr__();
    while (match(TokenType::IDENTIFIER) || match(TokenType::LEFT_PARENTHESIS) ||
           match(TokenType::CASE) || match(TokenType::FFI)) {
      auto t1 = expr;
      expr = parse_expr__();
      auto t2 = make_shared<Expr>();
      t2->T = ExprType::APP;
      t2->e1 = t1;
      t2->e2 = expr;
      expr = t2;
    }
    return expr;
  }
  shared_ptr<Expr> parse_expr__() {
    auto expr = make_shared<Expr>();
    if (accept(TokenType::IDENTIFIER)) {
      expr->T = ExprType::VAR;
      expr->x = t.data;
    } else if (accept(TokenType::LEFT_PARENTHESIS)) {
      expr = parse_expr();
      expect(TokenType::RIGHT_PARENTHESIS);
    } else if (accept(TokenType::CASE)) {
      expr->T = ExprType::CASE;
      expr->e = parse_expr();
      expect(TokenType::OF);
      shared_ptr<Poly_> g;
      if (accept(TokenType::COLON)) {
        map<string, shared_ptr<Mono_>> m;
        g = parse_sig(m);
      }
      expr->gadt = g;
      expect(TokenType::LEFT_BRACE);
      string data_name;
      do {
        expect(TokenType::IDENTIFIER);
        if (!constructor_decl->count(t.data)) {
          string data = t.data;
          if (data.length() > 78) {
            data = data.substr(0, 75) + "...";
          }
          cerr << "parser: " << t.position << " constructor not found" << endl
               << "`" << data << "`" << endl;
          exit(EXIT_FAILURE);
        }
        if (expr->pes.count(t.data)) {
          string data = t.data;
          if (data.length() > 78) {
            data = data.substr(0, 75) + "...";
          }
          cerr << "parser: " << t.position << " constructors conflict" << endl
               << "`" << data << "`" << endl;
          exit(EXIT_FAILURE);
        }
        string cname = t.data;
        expr->pes[cname] = make_pair(vector<string>{}, nullptr);
        auto &pes = expr->pes[cname];
        auto c = (*constructor_decl)[cname];
        if (data_name.empty()) {
          data_name = c->data_name;
        } else if (data_name != c->data_name) {
          string data = t.data;
          if (data.length() > 78) {
            data = data.substr(0, 75) + "...";
          }
          cerr << "parser: " << t.position << " constructor of " << data_name
               << " expected, but found" << endl
               << "`" << data << "`" << endl;
          exit(EXIT_FAILURE);
        }
        set<string> st;
        for (size_t i = 0; i < c->arg; i++) {
          expect(TokenType::IDENTIFIER);
          if (st.count(t.data)) {
            string data = t.data;
            if (data.length() > 78) {
              data = data.substr(0, 75) + "...";
            }
            cerr << "parser: " << t.position << " variable names conflict"
                 << endl
                 << "`" << data << "`" << endl;
            exit(EXIT_FAILURE);
          }
          st.insert(t.data);
          pes.first.push_back(t.data);
        }
        expect(TokenType::RIGHTARROW);
        pes.second = parse_expr();
        if (!match(TokenType::RIGHT_BRACE)) {
          expect(TokenType::SEMICOLON);
        }
      } while (!match(TokenType::RIGHT_BRACE));
      expect(TokenType::RIGHT_BRACE);
      if (g != nullptr) {
        check(data_decl, expr->xes.back().second);
      }
    } else if (accept(TokenType::FFI)) {
      // TODO support typeof() FIXME ffi must have a sig
      expr->T = ExprType::FFI;
      stringstream s(t.data);
      s.get();
      s.get();
      s.get();
      string sep;
      s >> sep;
      size_t a = t.data.find(sep);
      expr->ffi =
          t.data.substr(a + sep.size(), t.data.size() - (a + 2 * sep.size()));
    }
    return expr;
  }
  pair<pair<shared_ptr<map<string, shared_ptr<Data>>>,
            shared_ptr<map<string, shared_ptr<Constructor>>>>,
       shared_ptr<Expr>>
  parse() {
    data_decl = make_shared<map<string, shared_ptr<Data>>>();
    constructor_decl = make_shared<map<string, shared_ptr<Constructor>>>();
    while (accept(TokenType::DATA)) {
      parse_data();
    }
    check(data_decl, constructor_decl);
    auto expr = parse_expr();
    expect(TokenType::END);
    return make_pair(make_pair(data_decl, constructor_decl), expr);
  }
};

#endif
