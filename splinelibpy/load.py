"""splinelibpy/splineplibpy/load.py

Single function file containing `load_splines`.
"""

import os

from splinelibpy._splinelibpy import Reader
from splinelibpy.bspline import BSpline
from splinelibpy.nurbs import NURBS
from splinelibpy.utils import abs_fname

def load_splines(fname):
    """
    Loads spline files of extension 
      - `.iges`
      - `.xml`
      - `.itd`
      - `.npz`

    Parameters
    -----------
    fname: str

    Returns
    --------
    splines: list of BSpline or/and NURBS
    """
    fname = str(fname)
    fname = abs_fname(fname)

    sr = Reader()

    ext = os.path.splitext(fname)[1]
    
    if ext == ".iges":
        loaded_splines = sr.read_iges(fname)
    elif ext == ".xml":
        loaded_splines = sr.read_xml(fname)
    elif ext == ".itd":
        loaded_splines = sr.read_irit(fname)
    elif ext == ".npz":
        npz = np.load(fname)
        whatami = npz["whatami"][0]
        if whatami.startswith("NURBS"):
            spline = NURBS()
            spline.weights = npz["weights"]
        else:
            spline = BSpline()

        spline.control_points = npz["control_points"]
        spline.degrees = npz["degrees"]
        spline.knot_vectors = eval(npz["knot_vectors"][0])

        # For now, npz only has 1 spline. However, to keep the output format
        # consistent, return a list.
        return [spline]

    else:
        raise ValueError(
            "We can only import < .iges | .xml | .itd > spline files"
        )

    splines = []
    # Format s => [weights, degrees, knot_vectors, control_points]
    for s in loaded_splines:
        if s[0] is None:
            # BSpline.
            tmp_spline = BSpline()
            tmp_spline.degrees = s[1]
            tmp_spline.knot_vectors = s[2]
            tmp_spline.control_points = s[3]
            splines.append(tmp_spline)
 
        else:
            # Make nurbs
            tmp_spline = NURBS()
            tmp_spline.weights = s[0]
            tmp_spline.degrees = s[1]
            tmp_spline.knot_vectors = s[2]
            tmp_spline.control_points = s[3]
            splines.append(tmp_spline)

    return splines
