#ifndef RECBART_H
#define RECBART_H

#include <RcppArmadillo.h>
#include "functions.h"
#include "slice.h"

// THINGS TO CHANGE WHEN ADDING NEW FEATURES:
// - The parameters in Hypers should be changed to reflect the current situation
// - By design, none of the changes to tree topology will need to be changed
// - MyData should be changed to reflect changes to the format of the data
//   - My Data includes also the estimated forest predictions (or, leave-one-out predictions)
//     to be used in the algorithm. It might contain additional information; the important
//     point is that it includes the relevant leave-one-out information to be used when
//     computed LogLT
// - In general, LogLT will need to be changed to reflect what is going on in the model
// - Other than these changes, you will also need to write new updates for the parameters.
//   - These type of updates are Node::UpdateMu()
//   - Depending on how predictions are being done, predict may need to be changed
// - Depending on the nature of the backfitting, you will also need to change
//   TreeBackfit, IterateGibbsNoS, and IterateGibbsWithS
// - The change to TreeBackfit is done by changing the functions Backfit and Refit
// - Need to change SuffStats, which is also model specific

struct Hypers;
struct Node;
struct MyData;


struct Hypers {

  // Tree parameters
  double alpha;
  double gamma;
  double beta;
  int num_trees;

  // Classification parameters
  double theta_01;
  double theta_02;
  double sigma_theta1;
  double sigma_theta2;

  // Alpha hyperparameters
  double alpha_scale;
  double alpha_shape_1;
  double alpha_shape_2;

  // Variance hyperparameters
  double sigma_theta_hat1;
  double sigma_theta_hat2;


  // splitting prob parameters
  arma::vec s;
  arma::vec logs;

  // Grouping
  int num_groups;
  arma::uvec group;
  std::vector<std::vector<unsigned int> > group_to_vars;

  // Constructor
  Hypers(const arma::mat& X, const arma::uvec& group, Rcpp::List hypers);

  // Functions
  void UpdateAlpha();
  void UpdateTau(MyData& mydata);
  int SampleVar() const;

};

struct MyData {
  arma::mat W;
  arma::uvec delta1;
  arma::uvec delta2;
  arma::vec Z1;
  arma::vec Z2;
  arma::vec theta_hat1;
  arma::vec theta_hat2;

  MyData(arma::mat& Ww, 
         arma::uvec& deltax1,
         arma::uvec& deltax2,  double theta_01, double theta_02)
  :  W(Ww), delta1(deltax1), delta2(deltax2) {

    theta_hat1 = theta_01 + arma::zeros<arma::vec>(Ww.n_rows);
    theta_hat2 = theta_02 + arma::zeros<arma::vec>(Ww.n_rows);
    Z1 = arma::zeros<arma::vec>(delta1.n_elem);
    Z2 = arma::zeros<arma::vec>(delta2.n_elem);

    for(int i = 0; i < delta1.n_elem; i++) {//lrf assuming same dim
      if(delta1(i) == 0) {
        Z1(i) = randnt(theta_hat1(i), 1.0, R_NegInf, 0.0);
      } else {
        Z1(i) = randnt(theta_hat1(i), 1.0, 0.0, R_PosInf);
      }
      if(delta2(i) == 0) {
        Z2(i) = randnt(theta_hat2(i), 1.0, R_NegInf, 0.0);
      } else {
        Z2(i) = randnt(theta_hat2(i), 1.0, 0.0, R_PosInf);
      }
    }
  }
};

struct SuffStats {

  double sum_Z1;
  double sum_Z_sq1;
  double n_Z1;
  
  double sum_Z2;
  double sum_Z_sq2;
  double n_Z2;

SuffStats() : sum_Z1(0.0), sum_Z_sq1(0.0), sum_Z2(0.0), sum_Z_sq2(0.0),  n_Z1(0.0), n_Z2(0.0){;}

};

struct Node {
  bool is_leaf;
  bool is_root;
  Node* left;
  Node* right;
  Node* parent;

  // Branch parameters
  int var;
  double val;
  double lower;
  double upper;
  int depth;

  // Leaf parameters
  
  double theta1;
  double theta2;

  SuffStats ss;

  // Hyperparameters
  Hypers* hypers;

  void Root();
  void GetLimits();
  void BirthLeaves();
  bool is_left();
  void DeleteLeaves();
  void UpdateParams(MyData& data);
  double LogLT(const MyData& data);
  void UpdateSuffStat(const MyData& data);
  void ResetSuffStat();
  void AddSuffStatZ(const MyData& data, int i);

  Node(Hypers* hypers_);
  Node(Node* parent);
  ~Node();
};

struct Opts {
  int num_burn;
  int num_thin;
  int num_save;
  int num_print;

  bool update_s;
  bool update_alpha;

  Opts() : update_s(true), update_alpha(true) {

    num_burn = 1;
    num_thin = 1;
    num_save = 1;
    num_print = 100;

  }

  Opts(int nburn, int nthin, int nsave,
       int nprint, bool updates, bool updatealpha) :
  num_burn(nburn), num_thin(nthin), num_save(nsave), num_print(nprint),
  update_s(updates), update_alpha(updatealpha) {;}

  Opts(Rcpp::List opts) {
    num_burn = opts["num_burn"];
    num_thin = opts["num_thin"];
    num_save = opts["num_save"];
    num_print = opts["num_print"];
    update_s = opts["update_s"];
    update_alpha = opts["update_alpha"];
  }
};



// Node functions
double growth_prior(Node* node);
int depth(Node* node);
void leaves(Node* x, std::vector<Node*>& leafs);
std::vector<Node*> leaves(Node* x);
std::vector<Node*> not_grand_branches(Node* tree);
void not_grand_branches(std::vector<Node*>& ngb, Node* node);
Node* rand(std::vector<Node*> ngb);
arma::uvec get_var_counts(std::vector<Node*>& forest, const Hypers& hypers);
void get_var_counts(arma::uvec& counts, Node* node, const Hypers& hypers);
double forest_loglik(std::vector<Node*>& forest);
double tree_loglik(Node* node);
std::vector<Node*> init_forest(Hypers& hypers);

/* arma::vec predict(Node* n, const arma::mat& X, const Hypers& hypers); */
/* double predict(Node* n, const arma::rowvec& x); */

// Tree MCMC
double probability_node_birth(Node* tree);
Node* birth_node(Node* tree, double* leaf_node_probability);
Node* death_node(Node* tree, double* p_not_grand);
void birth_death(Node* tree, const MyData& data);
void node_birth(Node* tree, const MyData& data);
void node_death(Node* tree, const MyData& data);
void change_decision_rule(Node* tree, const MyData& data);

// Other MCMC
void UpdateS(std::vector<Node*>& forest);
void UpdateZ(MyData& data);
void UpdateSigmaParam(std::vector<Node*>& forest);
arma::mat get_params(std::vector<Node*>& forest);
void get_params(Node* n, std::vector<double>& theta1,std::vector<double>& theta2);


/* arma::vec loglik_data(const arma::vec& Y, const arma::vec& rho, const Hypers& hypers); */
void IterateGibbsNoS(std::vector<Node*>& forest,
                     MyData& data,
                     const Opts& opts);
void IterateGibbsWithS(std::vector<Node*>& forest,
                       MyData& data,
                       const Opts& opts);
void do_burn_in(std::vector<Node*> forest, MyData& data, const Opts& opts);
void TreeBackfit(std::vector<Node*>& forest,
                 MyData& mydata,
                 const Opts& opts);
void BackFit(Node* tree, MyData& data);
void Refit(Node* tree, MyData& data);

// Predictions

arma::mat predict_theta(Node* tree, MyData& data);
arma::mat predict_theta(Node* tree, arma::mat& W);
arma::rowvec predict_theta(Node* tree, arma::rowvec& x);
arma::mat predict_theta(std::vector<Node*> forest, arma::mat& W);


#endif
