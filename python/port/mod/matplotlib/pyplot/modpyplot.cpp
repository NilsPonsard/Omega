extern "C" {
#include "modpyplot.h"
}
#include <assert.h>
#include <escher/palette.h>
#include "port.h"
#include "plot_controller.h"

Matplotlib::PlotStore * sPlotStore = nullptr;
Matplotlib::PlotController * sPlotController = nullptr;
static int paletteIndex = 0;

// Private helper

// Method to populate items with a scalar or an array argument

static size_t extractArgument(mp_obj_t arg, mp_obj_t ** items) {
  size_t itemLength;
  if (mp_obj_is_type(arg, &mp_type_tuple) || mp_obj_is_type(arg, &mp_type_list)) {
    mp_obj_get_array(arg, &itemLength, items);
  } else {
    itemLength = 1;
    *items = m_new(mp_obj_t, 1);
    (*items)[0] = arg;
  }
  return itemLength;
}

// Extract two scalar or array arguments and check for their strickly equal dimension

static size_t extractArgumentsAndCheckEqualSize(mp_obj_t x, mp_obj_t y, mp_obj_t ** xItems, mp_obj_t ** yItems) {
  size_t xLength = extractArgument(x, xItems);
  size_t yLength = extractArgument(y, yItems);
  if (xLength != yLength) {
    mp_raise_ValueError("x and y must be the same size");
  }
  return xLength;
}

/* Extract one scalar or array arguments and check that it is either:
 * - of size 1
 * - of the required size
*/

size_t extractArgumentAndValidateSize(mp_obj_t arg, size_t requiredlength, mp_obj_t ** items) {
  size_t itemLength = extractArgument(arg, items);
  if (itemLength > 1 && requiredlength > 1 && itemLength != requiredlength) {
    mp_raise_ValueError("shape mismatch");
  }
  return itemLength;
}

// Internal functions

mp_obj_t modpyplot___init__() {
  static Matplotlib::PlotStore plotStore;
  static Matplotlib::PlotController plotController(&plotStore);
  sPlotStore = &plotStore;
  sPlotController = &plotController;
  sPlotStore->flush();
  paletteIndex = 0;
  return mp_const_none;
}

void modpyplot_gc_collect() {
  if (sPlotStore == nullptr) {
    return;
  }
  MicroPython::collectRootsAtAddress(
    reinterpret_cast<char *>(sPlotStore),
    sizeof(Matplotlib::PlotStore)
  );
}

void modpyplot_flush_used_heap() {
  if (sPlotStore) {
    // Clean the store object
    sPlotStore->flush();
  }
}

/* arrow(x,y,dx,dy)
 * x, y, dx, dy scalars
 * */

mp_obj_t modpyplot_arrow(size_t n_args, const mp_obj_t *args) {
  assert(n_args == 4);
  assert(sPlotStore != nullptr);

  KDColor color = Palette::nextDataColor(&paletteIndex);
  sPlotStore->addSegment(args[0], args[1], mp_obj_new_float(mp_obj_get_float(args[0])+mp_obj_get_float(args[2])), mp_obj_new_float(mp_obj_get_float(args[1])+mp_obj_get_float(args[3])), color, true); // TODO: use float_binary_op

  return mp_const_none;
}

/* axis(arg)
 *  - arg = "on", "off", "auto"
 *  - arg = True, False
 *  - arg = [xmin, xmax, ymin, ymax], (xmin, xmax, ymin, ymax)
 * Returns : (xmin, xmax, ymin, ymax) : float */

mp_obj_t modpyplot_axis(size_t n_args, const mp_obj_t *args) {
  assert(sPlotStore != nullptr);

  if (n_args == 1) {
    mp_obj_t arg = args[0];
    if (mp_obj_is_str(arg)) {
      if (mp_obj_str_equal(arg, mp_obj_new_str("on", 2))) {
        sPlotStore->setAxesRequested(true);
      } else if (mp_obj_str_equal(arg, mp_obj_new_str("off", 3))) {
        sPlotStore->setAxesRequested(false);
      } else if (mp_obj_str_equal(arg, mp_obj_new_str("auto", 4))) {
        sPlotStore->setAxesRequested(true);
        sPlotStore->setAxesAuto(true);
      } else {
        mp_raise_ValueError("Unrecognized string given to axis; try 'on', 'off' or 'auto'");
      }
#warning Use mp_obj_is_bool when upgrading uPy
    } else if (mp_obj_is_type(arg, &mp_type_bool)) {
      sPlotStore->setAxesRequested(mp_obj_is_true(arg));
    } else if (mp_obj_is_type(arg, &mp_type_tuple) || mp_obj_is_type(arg, &mp_type_list)) {
      mp_obj_t * items;
      mp_obj_get_array_fixed_n(arg, 4, &items);
      sPlotStore->setXMin(mp_obj_get_float(items[0]));
      sPlotStore->setXMax(mp_obj_get_float(items[1]));
      sPlotStore->setYMin(mp_obj_get_float(items[2]));
      sPlotStore->setYMax(mp_obj_get_float(items[3]));
      sPlotStore->setAxesAuto(false);
    } else {
      mp_raise_TypeError("the first argument to axis() must be an iterable of the form [xmin, xmax, ymin, ymax]");
    }
  }

  // Build the return value
  mp_obj_t coords[4];
  coords[0] = mp_obj_new_float(sPlotStore->xMin());
  coords[1] = mp_obj_new_float(sPlotStore->xMax());
  coords[2] = mp_obj_new_float(sPlotStore->yMin());
  coords[3] = mp_obj_new_float(sPlotStore->yMax());
  return mp_obj_new_tuple(4, coords);
}

/* bar(x, height, width, bottom)
 * 'x', 'height', 'width' and 'bottom' can either be a scalar or an array/tuple of
 * scalar.
 * 'width' default value is 0.8
 * 'bottom' default value is None
 * */

// TODO: accept keyword args?

mp_obj_t modpyplot_bar(size_t n_args, const mp_obj_t *args) {
  assert(sPlotStore != nullptr);

  mp_obj_t * xItems;
  mp_obj_t * hItems;
  mp_obj_t * wItems;
  mp_obj_t * bItems;

  // x arg
  size_t xLength = extractArgument(args[0], &xItems);

  // height arg
  size_t hLength = extractArgumentAndValidateSize(args[1], xLength, &hItems);

  // width arg
  size_t wLength = 1;
  if (n_args >= 3) {
    wLength = extractArgumentAndValidateSize(args[2], xLength, &wItems);
  } else {
    wItems = m_new(mp_obj_t, 1);
    wItems[0] = mp_obj_new_float(0.8f);
  }

  // bottom arg
  size_t bLength = 1;
  if (n_args >= 4) {
    bLength = extractArgumentAndValidateSize(args[3], xLength, &bItems);
  } else {
    bItems = m_new(mp_obj_t, 1);
    bItems[0] = mp_obj_new_float(0.0f);
  }

  KDColor color = Palette::nextDataColor(&paletteIndex);
  for (size_t i=0; i<xLength; i++) {
    mp_float_t iH = mp_obj_get_float(hItems[hLength > 1 ? i : 0]);
    mp_float_t iW = mp_obj_get_float(wItems[wLength > 1 ? i : 0]);
    mp_float_t iB = mp_obj_get_float(bItems[bLength > 1 ? i : 0]);
    mp_float_t iX = mp_obj_get_float(xItems[i])-iW/2.0;
    mp_float_t iYStart = iH < 0.0 ? iB : iB + iH;
    mp_float_t iYEnd = iH < 0.0 ? iB + iH : iB;
    sPlotStore->addRect(mp_obj_new_float(iX), mp_obj_new_float(iX+iW), mp_obj_new_float(iYStart), mp_obj_new_float(iYEnd), color); // TODO: use float_binary_op?
  }
  return mp_const_none;
}

mp_obj_t modpyplot_grid(size_t n_args, const mp_obj_t *args) {
  assert(sPlotStore != nullptr);

  if (n_args == 0) {
    // Toggle the grid visibility
    sPlotStore->setGridRequested(!sPlotStore->gridRequested());
  } else {
    sPlotStore->setGridRequested(mp_obj_is_true(args[0]));
  }
  return mp_const_none;
}

/* hist(x, bins)
 * 'x' array
 * 'bins': (default value 10)
 *    - int (number of bins)
 *    - sequence of bins
 * */

mp_obj_t modpyplot_hist(size_t n_args, const mp_obj_t *args) {
  assert(sPlotStore != nullptr);

  // Sort data to easily get the minimal and maximal value and count bin sizes
  mp_obj_t * xItems;
  size_t xLength = extractArgument(args[0], &xItems);
  if (xLength == 0) {
    return mp_const_none;
  }
  mp_obj_t xList = mp_obj_new_list(xLength, xItems);
  mp_obj_list_sort(1, &xList, (mp_map_t*)&mp_const_empty_map);
  mp_obj_list_get(xList, &xLength, &xItems);
  assert(xLength > 0);
  mp_float_t min = mp_obj_get_float(xItems[0]);
  mp_float_t max = mp_obj_get_float(xItems[xLength - 1]);

  mp_obj_t * edgeItems;
  size_t nBins;
  // bin arg
  if (n_args >= 2 && (mp_obj_is_type(args[1], &mp_type_tuple) || mp_obj_is_type(args[1], &mp_type_list))) {
    size_t nEdges;
    mp_obj_get_array(args[1], &nEdges, &edgeItems);
    nBins = nEdges -1;
  } else {
    nBins = 10;
    if (n_args >= 2) {
      nBins = mp_obj_get_int(args[1]);
    }

    mp_float_t binWidth = (max-min)/nBins;
    // Create a array of bins
    edgeItems = m_new(mp_obj_t, nBins + 1);
    // Handle empty range case
    if (max - min <= FLT_EPSILON) {
      binWidth = 1.0;
      nBins = 1;
    }

    // Fill the bin edges list
    for (int i = 0; i < nBins+1; i++) {
      edgeItems[i] = mp_obj_new_float(min+i*binWidth);
    }
  }

  // Initialize bins list
  mp_obj_t * binItems = m_new(mp_obj_t, nBins);
  for (size_t i=0; i<nBins; i++) {
    binItems[i] = MP_OBJ_NEW_SMALL_INT(0);
  }

  // Fill bins list by linearly scanning the x and incrementing the bin count
  // Linearity is enabled thanks to sorting
  size_t binIndex = 0;
  size_t xIndex = 0;
  while (binIndex < nBins) {
    mp_float_t lowerBound = mp_obj_get_float(edgeItems[binIndex]);
    // Skip xItem if below the lower bound
    while (xIndex < xLength && mp_obj_get_float(xItems[xIndex]) < lowerBound) {
      xIndex++;
    }
    mp_float_t upperBound = mp_obj_get_float(edgeItems[binIndex+1]);
    while (xIndex < xLength && (mp_obj_get_float(xItems[xIndex]) < upperBound || (binIndex == nBins - 1 && mp_obj_get_float(xItems[xIndex]) == upperBound))) {
      assert(mp_obj_get_float(xItems[xIndex]) >= lowerBound);
      // Increment the bin count
      binItems[binIndex] = MP_OBJ_NEW_SMALL_INT(MP_OBJ_SMALL_INT_VALUE(binItems[binIndex]) + 1);
      xIndex++;
    }
    binIndex++;
  }

  KDColor color = Palette::nextDataColor(&paletteIndex);
  for (size_t i=0; i<nBins; i++) {
    sPlotStore->addRect(edgeItems[i], edgeItems[i+1], binItems[i], mp_obj_new_float(0.0), color);
  }
  return mp_const_none;
}

/* scatter(x, y)
 * - x, y: list
 * - x, y: scalar
 * */

mp_obj_t modpyplot_scatter(mp_obj_t x, mp_obj_t y) {
  assert(sPlotStore != nullptr);

  mp_obj_t * xItems, * yItems;
  size_t length = extractArgumentsAndCheckEqualSize(x, y, &xItems, &yItems);

  KDColor color = Palette::nextDataColor(&paletteIndex);
  for (size_t i=0; i<length; i++) {
    sPlotStore->addDot(xItems[i], yItems[i], color);
  }

  return mp_const_none;
}

/* plot(x, y) plots the curve (x, y)
 * plot(y) plots the curve x as index array ([0,1,2...],y)
 * */

mp_obj_t modpyplot_plot(size_t n_args, const mp_obj_t *args) {
  assert(sPlotStore != nullptr);

  mp_obj_t * xItems, * yItems;
  size_t length;
  if (n_args == 1) {
    length = extractArgument(args[0], &yItems);

    // Create the default xItems: [0, 1, 2,...]
    xItems = m_new(mp_obj_t, length);
    for (int i = 0; i < length; i++) {
      xItems[i] = mp_obj_new_float((float)i);
    }
  } else {
    assert(n_args == 2);
    length = extractArgumentsAndCheckEqualSize(args[0], args[1], &xItems, &yItems);
  }

  KDColor color = Palette::nextDataColor(&paletteIndex);
  for (int i=0; i<(int)length-1; i++) {
    sPlotStore->addSegment(xItems[i], yItems[i], xItems[i+1], yItems[i+1], color, false);
  }

  return mp_const_none;
}

mp_obj_t modpyplot_text(mp_obj_t x, mp_obj_t y, mp_obj_t s) {
  assert(sPlotStore != nullptr);

  // Input parameter validation
  mp_obj_get_float(x);
  mp_obj_get_float(y);
  mp_obj_str_get_str(s);

  sPlotStore->addLabel(x, y, s);

  return mp_const_none;
}

mp_obj_t modpyplot_show() {
  if (sPlotStore->isEmpty()) {
    return mp_const_none;
  }
  MicroPython::ExecutionEnvironment * env = MicroPython::ExecutionEnvironment::currentExecutionEnvironment();
  env->displayViewController(sPlotController);
  return mp_const_none;
}
