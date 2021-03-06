#include <poincare/undefined.h>
#include <poincare/complex.h>
#include <poincare/layout_helper.h>

extern "C" {
#include <math.h>
#include <string.h>
}

namespace Poincare {

static inline int minInt(int x, int y) { return x < y ? x : y; }

int UndefinedNode::polynomialDegree(Context * context, const char * symbolName) const {
  return -1;
}

Expression UndefinedNode::setSign(Sign s, ExpressionNode::ReductionContext reductionContext) {
  assert(s == ExpressionNode::Sign::Positive || s == ExpressionNode::Sign::Negative);
  return Undefined(this);
}

Layout UndefinedNode::createLayout(Preferences::PrintFloatMode floatDisplayMode, int numberOfSignificantDigits) const {
  return LayoutHelper::String(Undefined::Name(), Undefined::NameSize()-1);
}

int UndefinedNode::serialize(char * buffer, int bufferSize, Preferences::PrintFloatMode floatDisplayMode, int numberOfSignificantDigits) const {
  return minInt(strlcpy(buffer, Undefined::Name(), bufferSize), bufferSize - 1);
}

template<typename T> Evaluation<T> UndefinedNode::templatedApproximate() const {
  return Complex<T>::Undefined();
}

template Evaluation<float> UndefinedNode::templatedApproximate() const;
template Evaluation<double> UndefinedNode::templatedApproximate() const;
}

