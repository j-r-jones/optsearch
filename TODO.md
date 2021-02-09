## TODO
* The current data representation is overly simplistic and not very
  efficient.  A sparse representation, where we keep two vectors of values, one giving any values that are set (not any that are not) and one corresponding to which flags these values correspond to, along with a lookup table, would be much better.  This should be carried through to the database.  This would later allow the replacement of the optimisation algorithm if there is a better one for this specific search space.  It also means that we can shorten the output more easily, and consider finding a minimal set of flags rather than the first set that works.

