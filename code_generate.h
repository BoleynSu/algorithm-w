#ifndef SU_BOLEYN_BSL_CODE_GENERATE_H
#define SU_BOLEYN_BSL_CODE_GENERATE_H

#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "data.h"
#include "expr.h"
#include "optimize.h"
#include "parse.h"
#include "type.h"
#include "type_infer.h"

using namespace std;

const string BSL_RT_CLOSURE_T = "BSL_RT_CLOSURE_T";
const string BSL_RT_FUN_T = "BSL_RT_FUN_T";
const string BSL_RT_VAR_T = "BSL_RT_VAR_T";
const string BSL_RT_CALL = "BSL_RT_CALL";

const string BSL_TYPE_ = "BSL_TYPE_";
const string BSL_TAG_ = "BSL_TAG_";
const string BSL_CON_ = "BSL_CON_";
const string BSL_FUN_ = "BSL_FUN_";
const string BSL_VAR_ = "BSL_VAR_";
const string BSL_ENV = "BSL_ENV";

struct CodeGenerator {
  Parser& parser;
  ostream& out;
  vector<shared_ptr<stringstream>> fns;
  set<size_t> cons;
  CodeGenerator(Parser& parser, ostream& out) : parser(parser), out(out) {
    codegen(parser.parse());
  }

  string var(string v, const map<string, size_t>& env = map<string, size_t>()) {
    string nv;
    for (size_t i = 0; i < v.length(); i++) {
      if (v[i] == '_') {
        nv.push_back('_');
        nv.push_back('_');
      } else if (v[i] == '\'') {
        nv.push_back('_');
        nv.push_back('0');
      } else {
        nv.push_back(v[i]);
      }
    }
    if (env.count(nv)) {
      size_t idx = env.find(nv)->second;
      stringstream ss;
      ss << BSL_ENV << "[" << idx << "]";
      return ss.str();
    } else {
      return BSL_VAR_ + nv;
    }
  }
  string tmp() { return BSL_VAR_ + "_1"; }
  string type(const string& t) { return BSL_TYPE_ + t; }
  string tag(const string& t) { return BSL_TAG_ + t; }
  string con(const string& c) {
    stringstream ss;
    ss << BSL_CON_ << c;
    return ss.str();
  }
  string arg(size_t i) {
    stringstream ss;
    ss << "arg" << i;
    return ss.str();
  }
  string fun(size_t i) {
    stringstream ss;
    ss << BSL_FUN_ << i;
    return ss.str();
  }
  string ffi(const string& f,
             const map<string, size_t>& env = map<string, size_t>()) {
    stringstream s;
    size_t idx = 0;
    while (idx < f.length()) {
      if (f[idx] != '$') {
        s << f[idx++];
      } else if (++idx < f.length()) {
        char c = f[idx];
        if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_' ||
            c == '\'') {
          string v;
          v.push_back(c);
          while (++idx < f.length()) {
            c = f[idx];
            if (!(('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') ||
                  ('a' <= c && c <= 'z') || c == '_' || c == '\'')) {
              break;
            }
            v.push_back(c);
          }
          s << var(v, env);
        } else {
          s << '$' << f[idx++];
        }
      }
    }
    return s.str();
  }

  void codegen(ostream& out, shared_ptr<Expr> expr,
               const map<string, size_t>& env) {
    switch (expr->T) {
      case ExprType::VAR: {
        out << var(expr->x, env);
        return;
      }
      case ExprType::APP: {
        out << BSL_RT_CALL << "(";
        codegen(out, expr->e1, env);
        out << ",";
        codegen(out, expr->e2, env);
        out << ")";
      }
        return;
      case ExprType::ABS: {
        map<string, size_t> env;
        for (auto& fv : expr->fv) {
          env.insert(make_pair(fv, env.size()));
        }
        size_t fn_idx = fns.size();
        fns.push_back(make_shared<stringstream>());
        auto& nout = *fns.back();
        nout << BSL_RT_VAR_T << " " << fun(fn_idx) << "(" << BSL_RT_VAR_T << " "
             << var(expr->x) << ", " << BSL_RT_VAR_T << " " << BSL_ENV
             << "[]) {" << endl;
        nout << " return ";
        codegen(nout, expr->e, env);
        nout << ";" << endl << "}" << endl;

        cons.insert(env.size());
        out << BSL_CON_ << env.size() << "(" << fun(fn_idx);
        for (auto& fv : expr->fv) {
          out << ", " << var(fv, env);
        }
        out << ")";
      }
        return;
      case ExprType::LET: {
        map<string, size_t> env, env1, env2;
        for (auto& fv : expr->fv) {
          env.insert(make_pair(fv, env.size()));
        }
        for (auto& fv : expr->e1->fv) {
          env1[fv] = env[fv];
        }
        for (auto& fv : expr->e2->fv) {
          if (fv != expr->x) {
            env2[fv] = env[fv];
          }
        }
        size_t fn_idx = fns.size();
        fns.push_back(make_shared<stringstream>());
        auto& nout = *fns.back();
        nout << BSL_RT_VAR_T << " " << fun(fn_idx) << "(" << BSL_RT_VAR_T << " "
             << var(expr->x) << ", " << BSL_RT_VAR_T << " " << BSL_ENV
             << "[]) {" << endl;
        nout << "  return ";
        codegen(nout, expr->e2, env2);
        nout << ";" << endl << "}" << endl;

        cons.insert(env.size());
        out << BSL_RT_CALL << "(" << BSL_CON_ << env.size() << "("
            << fun(fn_idx);
        for (auto& fv : expr->fv) {
          out << ", " << var(fv, env);
        }
        out << "), ";
        codegen(out, expr->e1, env1);
        out << ")";
      }
        return;
      case ExprType::REC: {  // TODO FIXME rec x = x + 1
                             //        out << "[=]() -> void* { void";
        //        for (size_t i = 0; i < expr->xes.size(); i++) {
        //          auto t = find(expr->xes[i].second->type);
        //          if (t->is_const) {
        //            out << (i ? ", *" : " *") << var(expr->xes[i].first) << "
        //            = new "
        //                << type(t->D) << "()";
        //          } else {
        //            cerr << "code generator:" << endl
        //                 << "rec " << expr->xes[i].first << " = "
        //                 << expr->xes[i].second->to_string() << " in ..." <<
        //                 endl;
        //            exit(EXIT_FAILURE);
        //          }
        //        }
        //        out << ";";
        //        for (size_t i = 0; i < expr->xes.size(); i++) {
        //          auto t = find(expr->xes[i].second->type);
        //          out << " new (" << var(expr->xes[i].first) << ") " <<
        //          type(t->D)
        //              << "(*((" << type(t->D) << "*)";
        //          codegen(expr->xes[i].second);
        //          out << "));";
        //        }
        //        out << " return ";
        //        codegen(expr->e);
        //        out << "; } ()";

        map<string, size_t> env, env2;
        for (auto& fv : expr->fv) {
          env.insert(make_pair(fv, env.size()));
        }
        for (size_t i = 0; i < expr->xes.size(); i++) {
          env.insert(make_pair(expr->xes[i].first, env.size()));
        }
        for (auto& fv : expr->e->fv) {
          env2[fv] = env[fv];
        }
        size_t fn_idx = fns.size();
        fns.push_back(make_shared<stringstream>());
        auto& nout = *fns.back();
        nout << BSL_RT_VAR_T << " " << fun(fn_idx) << "(" << BSL_RT_VAR_T << " "
             << "_, " << BSL_RT_VAR_T << " " << BSL_ENV << "[]) {" << endl;

        for (size_t i = 0; i < expr->xes.size(); i++) {
          map<string, size_t> env1;
          for (auto& fv : expr->xes[i].second->fv) {
            env1[fv] = env[fv];
          }
          auto t = find(expr->xes[i].second->type);
          if (t->is_const) {
            if (t->D == "->") {
              cons.insert(env1.size());
              nout << "  " << var(expr->xes[i].first, env1) << " = " << BSL_CON_
                   << env1.size() << "();";
            } else {
              nout << "  " << var(expr->xes[i].first, env1)
                   << " = malloc(sizeof(" << type(t->D) << "))";
            }
          } else {
            cerr << "code generator:" << endl
                 << "rec " << expr->xes[i].first << " = "
                 << expr->xes[i].second->to_string() << " in ..." << endl;
            exit(EXIT_FAILURE);
          }
        }
        nout << "  return ";
        codegen(nout, expr->e, env2);
        nout << ";" << endl << "}" << endl;

        cons.insert(env.size());
        out << BSL_RT_CALL << "(" << BSL_CON_ << env.size() << "("
            << fun(fn_idx);
        for (auto& fv : expr->fv) {
          out << ", " << var(fv, env);
        }
        out << "), NULL)";
      }
        return;
      case ExprType::CASE: {
      }
        return;
      case ExprType::FFI: {
        out << ffi(expr->ffi, env);
      }
        return;
    }
  }

  void codegen(pair<pair<shared_ptr<map<string, shared_ptr<Data>>>,
                         shared_ptr<map<string, shared_ptr<Constructor>>>>,
                    shared_ptr<Expr>>
                   prog) {
    auto data = prog.first.first;
    auto expr = prog.second;

    out << "#include <bsl_rt.h>" << endl;

    for (auto dai : *data) {
      auto da = dai.second;
      if (da->is_ffi) {
        out << "typedef " << ffi(da->ffi) << " " << type(da->name) << ";"
            << endl;
      } else {
        size_t maxarg = 0;
        for (size_t i = 0; i < da->constructors.size(); i++) {
          auto c = da->constructors[i];
          maxarg = max(maxarg, c->arg);
        }
        out << "typedef struct { ";
        if (da->constructors.size() > 1) {
          out << "enum {";
          for (size_t i = 0; i < da->constructors.size(); i++) {
            auto c = da->constructors[i];
            out << (i ? ", " : " ") << tag(c->name);
          }
          out << " } T; ";
        }
        for (size_t i = 0; i < maxarg; i++) {
          out << BSL_RT_VAR_T << " " << arg(i) << "; ";
        }
        out << "} " << type(da->name) << ";" << endl;
        for (size_t i = 0; i < da->constructors.size(); i++) {
          auto c = da->constructors[i];
          out << BSL_RT_VAR_T << " " << con(c->name) << "(";
          for (size_t j = 0; j < c->arg; j++) {
            out << (j == 0 ? "" : ", ") << BSL_RT_VAR_T << " " << var(arg(j));
          }
          out << ") {" << endl
              << "  " << type(da->name) << " *" << tmp() << " = malloc(sizeof("
              << type(da->name) << "));" << endl;
          if (da->constructors.size() > 1) {
            out << "  " << tmp() << "->T = " << tag(c->name) << ";" << endl;
          }
          for (size_t j = 0; j < c->arg; j++) {
            out << "  " << tmp() << "->" << arg(j) << " = " << var(arg(j))
                << ";" << endl;
          }
          out << "  return (" << BSL_RT_VAR_T << ")" << tmp() << ";" << endl;
          out << "}" << endl;
        }
      }
    }
    for (auto dai : *data) {
      auto da = dai.second;
      if (!da->is_ffi) {
        for (size_t i = 0; i < da->constructors.size(); i++) {
          auto c = da->constructors[i];
          auto e = make_shared<Expr>();
          e->T = ExprType::LET;
          e->x = c->name;
          auto lam = make_shared<Expr>();
          auto cur = lam;
          for (size_t j = 0; j < c->arg; j++) {
            cur->T = ExprType::ABS;
            cur->x = arg(j);
            cur->e = make_shared<Expr>();
            cur = cur->e;
          }
          stringstream s;
          s << " " << con(c->name) << "(";
          for (size_t j = 0; j < c->arg; j++) {
            s << (j == 0 ? "" : ", ") << "$" << arg(j);
          }
          s << ") ";
          cur->T = ExprType::FFI;
          cur->ffi = s.str();
          e->e1 = lam;
          e->e1->sig = c->type;
          e->e2 = expr;
          expr = e;
        }
      }
    }

    infer(expr, make_shared<map<string, shared_ptr<Poly>>>(), prog.first);

    stringstream main;
    codegen(main, optimize(expr), map<string, size_t>());

    for (size_t i : cons) {
      out << BSL_RT_VAR_T << " " << BSL_CON_ << i << "(" << BSL_RT_FUN_T
          << " fun";
      for (size_t j = 0; j < i; j++) {
        out << ", " << BSL_RT_VAR_T << " " << var(arg(j));
      }
      out << ") {" << endl
          << "  " << BSL_RT_CLOSURE_T << " " << tmp() << " = malloc(sizeof("
          << BSL_RT_FUN_T << ") + " << i << " * sizeof(" << BSL_RT_VAR_T
          << "));" << endl;
      out << "  " << tmp() << "->fun"
          << " = fun;" << endl;
      for (size_t j = 0; j < i; j++) {
        out << "  " << tmp() << "->env[" << j << "]"
            << " = " << var(arg(j)) << ";" << endl;
      }
      out << "  return (" << BSL_RT_VAR_T << ") " << tmp() << ";" << endl
          << "}" << endl;
    }

    for (size_t i = 0; i < fns.size(); i++) {
      out << BSL_RT_VAR_T << " " << fun(i) << "(" << BSL_RT_VAR_T << ", "
          << BSL_RT_VAR_T << "[]);" << endl;
    }
    for (auto fn : fns) {
      out << fn->str();
    }
    out << "int main() { " << main.str() << "; }" << endl;
  }
};

#endif
