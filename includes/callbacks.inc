<?php

/**
 * @file
 * Miscellaneous callbacks.
 */

/**
 * Page callback used for report page.
 *
 * Uses $_GET['format'], to determine how to render results... can be one of:
 * - "html": The default format, as an HTML table.
 * - "csv": The data in CSV format, as a download.
 */
function usc_mirc_reporting_pages_callback() {
  module_load_include('inc', 'usc_mirc_reporting', 'includes/reporting');

  return 'Reporting!';
}
