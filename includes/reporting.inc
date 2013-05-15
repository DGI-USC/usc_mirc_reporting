<?php

/**
 * @file
 * Miscellaneous functions to actually build the report structure.
 */

/**
 * Get the sizes of all assets of the given content model.
 *
 * @return array
 *   An array breaking down the size of all assets described in Fedora, with
 *   the structure:
 *   - period: One of:
 *     - "all":
 *     - "last_30_days"
 *     - "last_year"
 *     Furthermore, each period will be broken up into each of the separate
 *     content models, keyed according to that which is available in via the 
 *     variable_get().
 */
function usc_mirc_reporting_query_sizes() {

}
