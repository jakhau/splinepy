"""splinelibpy/splinelibpy/utils.py

Utility functions.
"""

import os

import numpy as np

from splinepy.log import debug 


def is_property(property_dict, key, class_name):
    """
    Checks if property exist in given dict. If not add a debug log.

    Parameters
    -----------
    property_dict: dict
    key: str
    class_name: str

    Returns
    --------
    is_property: bool
    """
    if key in property_dict:
        return True

    else:
        debug(
            class_name
            + " - `"
            + key
            + "` does not exist yet."
        )
        return False


def make_c_contiguous(array, dtype=None):
    """
    Make given array like object a c contiguous np.ndarray.
    dtype is optional.

    Parameters
    -----------
    array: array-like
    dtype: type or str
      (Optional) `numpy` interpretable type or str, describing type.
      Difference is that type will always return a copy and str will only copy
      if types doesn't match.

    Returns
    --------
    c_contiguous_array: np.ndarray
    """
    if isinstance(array, np.ndarray):
        if array.flags.c_contiguous:
            if dtype is not None:
                if isinstance(dtype, type):
                    return array.astype(dtype)

                elif isinstance(dtype, str):
                    if array.dtype.name != dtype:
                        return array.astype(dtype)

            return array

    if dtype:
        return np.ascontiguousarray(array, dtype=dtype)

    else:
        return np.ascontiguousarray(array)


def abs_fname(fname):
    """
    Checks if fname is absolute. If not, returns abspath. Tilde safe.

    Parameters
    ----------
    fname: str

    Returns
    --------
    abs_fname: str
      Maybe same to fname, maybe not.
    """
    if os.path.isabs(fname):
        pass

    elif "~" in fname:
        fname = os.path.expanduser(fname)

    else:
        fname = os.path.abspath(fname)

    return fname


def raster_points(
        bounds,
        resolutions,
):
    """
    Simple wraper of np.mgrid to extract raster points of desired bounds and
    resolutions.

    Parameters
    -----------
    bounds: (2, d) array-like
      float
    resolutions: (d,) array-like
      int. It will be casted to int.

    Returns
    --------
    raster_vertices: Vertices
    """
    if len(resolutions) != len(bounds[0]) == len(bounds[1]):
        raise ValueError("Length of resolutions and bounds should match.")

    points = "np.mgrid["
    for i, (b0, b1) in enumerate(zip(bounds[0], bounds[1])):
        points += f"{b0}:{b1}:{int(resolutions[i])}j,"
    points += "]"

    # Organize it nicely: 2D np.ndarray with shape (prod(resolutions), dim)
    points = eval(points).T.reshape(-1, len(resolutions))

    return points
