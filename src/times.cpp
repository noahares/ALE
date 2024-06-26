
#include "ALE_util.h"
#include "exODT.h"

#include <Bpp/Numeric/AutoParameter.h>
#include <Bpp/Numeric/Function/DownhillSimplexMethod.h>
#include <Bpp/Phyl/App/PhylogeneticsApplicationTools.h>
#include <Bpp/Phyl/OptimizationTools.h>

using namespace std;
using namespace bpp;

class p_fun : public virtual Function, public AbstractParametrizable {
private:
  double fval_;
  exODT_model *model_pointer;
  approx_posterior *ale_pointer;

public:
  p_fun(exODT_model *model, approx_posterior *ale, double delta_start = 0.01,
        double tau_start = 0.01,
        double lambda_start = 0.1 //,double sigma_hat_start=1.
        )
      : AbstractParametrizable(""), fval_(0), model_pointer(model),
        ale_pointer(ale) {
    // We declare parameters here:
    //   IncludingInterval* constraint = new IncludingInterval(1e-6, 10-1e-6);
    IntervalConstraint *constraint =
        new IntervalConstraint(1e-6, 10 - 1e-6, true, true);
    addParameter_(new Parameter("delta", delta_start, constraint));
    addParameter_(new Parameter("tau", tau_start, constraint));
    addParameter_(new Parameter("lambda", lambda_start, constraint));
    // addParameter_( new Parameter("sigma_hat", sigma_hat_start, constraint) )
    // ;
  }

  p_fun *clone() const { return new p_fun(*this); }

public:
  void setParameters(const ParameterList &pl) throw(ParameterNotFoundException,
                                                    ConstraintException,
                                                    Exception) {
    matchParametersValues(pl);
  }
  double getValue() const throw(Exception) { return fval_; }
  void fireParameterChanged(const ParameterList &pl) {
    double delta = getParameterValue("delta");
    double tau = getParameterValue("tau");
    double lambda = getParameterValue("lambda");
    // double sigma_hat = getParameterValue("sigma_hat");

    model_pointer->set_model_parameter("delta", delta);
    model_pointer->set_model_parameter("tau", tau);
    model_pointer->set_model_parameter("lambda", lambda);
    // model_pointer->set_model_parameter("sigma_hat",sigma_hat);
    model_pointer->calculate_EGb();
    double y = -log(model_pointer->p(ale_pointer));
    // cout <<endl<< "delta=" << delta << "\t tau=" << tau << "\t lambda=" <<
    // lambda //<< "\t lambda="<<sigma_hat << "\t ll="
    //     << -y <<endl;
    fval_ = y;
  }
};

int main(int argc, char **argv) {
  cout << "ALEml using ALE v" << ALE_VERSION << endl;

  if (argc < 3) {
    cout << "usage:\n ./ALEml species_tree.newick gene_tree_sample.ale "
            "[gene_name_separator]"
         << endl;
    return 1;
  }

  // we need a dated species tree in newick format
  string Sstring;
  ifstream file_stream_S(argv[1]);
  getline(file_stream_S, Sstring);
  cout << "Read species tree from: " << argv[1] << ".." << endl;
  // we need an .ale file containing observed conditional clade probabilities
  // cf. ALEobserve
  string ale_file = argv[2];
  approx_posterior *ale;
  ale = load_ALE_from_file(ale_file);
  cout << "Read summary of tree sample for " << ale->observations
       << " trees from: " << ale_file << ".." << endl;

  // we initialise a coarse grained reconciliation model for calculating the sum
  exODT_model *model = new exODT_model();

  int D = 3;
  if (argc > 3)
    model->set_model_parameter("gene_name_separators", argv[3]);
  model->set_model_parameter("BOOTSTRAP_LABELS", "yes");

  model->set_model_parameter("min_D", D);
  model->set_model_parameter("grid_delta_t", 0.05);
  model->construct(Sstring);

  model->set_model_parameter("event_node", 0);
  model->set_model_parameter("leaf_events", 1);
  model->set_model_parameter("N", 1);

  // a set of inital rates
  scalar_type delta = 0.01, tau = 0.01, lambda = 0.1;
  model->set_model_parameter("delta", delta);
  model->set_model_parameter("tau", tau);
  model->set_model_parameter("lambda", lambda);
  model->set_model_parameter("sigma_hat", 1);
  model->calculate_EGb();

  tree_type *T1 =
      TreeTemplateTools::parenthesisToTree(ale->constructor_string, false);
  vector<Node *> nodes1 = T1->getLeaves();
  map<string, int> names;
  for (vector<Node *>::iterator it = nodes1.begin(); it != nodes1.end(); it++) {
    vector<string> tokens;
    string name = (*it)->getName();
    boost::split(tokens, name, boost::is_any_of("_"), boost::token_compress_on);
    names[tokens[0]] += 1;
  }

  cout << T1->getNumberOfLeaves() << " " << names.size() << endl;

  boost::timer *t = new boost::timer();
  string outname = ale_file + ".times";
  ofstream fout(outname.c_str());
  model->p(ale);
  fout << t->elapsed() << "\t";
  fout << ale->Dip_counts.size() << "\t";
  fout << T1->getNumberOfLeaves() << "\t";
  fout << names.size() << "\t";
  fout << ale_file << endl;

  return 0;
}
