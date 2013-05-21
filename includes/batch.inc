<?php

/**
 * @file
 * Miscellaneous batch-related functions.
 */

/**
 * The number of objects to examine for file sizes at a time.
 */
define('USC_MIRC_REPORTING_BATCH_SIZE', 100);

/**
 * Setup the batch to calculate file sizes and the number of title.
 *
 * @param array $intervals
 *   An array of UNIX timestamps at which to sample.
 *
 * @return array
 *   An array containing the definition for a Drupal batch. Can pass directly
 *   to batch_set().
 */
function usc_mirc_reporting_report_batch(array $intervals) {
  // Get the content model URIs we use.
  $models = array(
    variable_get('usc_mirc_cmodel_access', 'usc:accessCModel'),
    variable_get('usc_mirc_cmodel_mezzanine', 'usc:mezzanineCModel'),
    variable_get('usc_mirc_cmodel_vro', 'usc:vroCModel'),
  );

  $batch = array(
    'title' => 'USC MIRC Report Generation',
    'finished' => 'usc_mirc_reporting_save_results',
    'operations' => array(
      array(
        'usc_mirc_reporting_batch_file_sizes_and_type_count',
        array(
          $intervals,
          $models,
        ),
      ),
      array(
        'usc_mirc_reporting_batch_title_counts',
        array(
          $intervals,
          $models,
        ),
      ),
    ),
    'file' => format_string('!module_path/!file_path', array(
      '!module_path' => drupal_get_path('module', 'usc_mirc_reporting'),
      '!file_path' => 'includes/batch.inc',
    )),
  );

  return $batch;
}

/**
 * Batch "finished" callback.
 *
 * Save the report into a variable.
 */
function usc_mirc_reporting_save_results($success, $results, $operations) {
  $report = $results['report'];

  if ($success) {
    $report['date'] = time();
    $report['intervals'] = array_keys($report['types']);
    variable_set('usc_mirc_reporting_report', $report);
  }
}

/**
 * Batch operation to report on file sizes.
 *
 * @param array $intervals
 *   An array of starting dates, which we will use to group the results.
 * @param array $models
 *   The array of content models, access, mezzanine and then vro.
 * @param mixed $context
 *   An array (or ArrayObject) of batch context. We will set
 *   $context['results']['report']['types'] to contain an array containing:
 *   - time: Either the string "all_time" or an integer denoting the
 *     beginning of an interval. Contains:
 *     - type: A string representing the type, one of those included in the
 *       $models parameter or the string "total". In turn, it contains:
 *       - "size": A string containing the aggregated size.
 *       - "count": An integer containing the count of items.
 */
function usc_mirc_reporting_batch_file_sizes_and_type_count($intervals, $models, &$context) {
  // Types to iterate... We keep track of the "total" here, in addition to the
  // the models themselves.
  $type_to_iterate = array('total');

  // Add the models to the types to iterate over (when (un)serializing).
  $type_to_iterate = array_merge($type_to_iterate, $models);

  $intervals_to_iterate = $intervals;
  if (!isset($context['sandbox']['offset'])) {
    // Some initialization.
    $context['sandbox']['offset'] = 0;
    foreach ($type_to_iterate as $type) {
      foreach ($intervals_to_iterate as $interval) {
        $context['results']['report']['types'][$interval][$type] = array(
          'size' => gmp_init(0, 10),
          'count' => 0,
        );
      }
    }
  }
  else {
    // Increment our offset, to get the next set.
    $context['sandbox']['offset'] += USC_MIRC_REPORTING_BATCH_SIZE;

    // Unserialize our big numbers.
    foreach ($context['results']['report']['types'] as $interval => &$type_report) {
      foreach ($type_to_iterate as $type) {
        $type_report[$type]['size'] = gmp_init($type_report[$type]['size'], 10);
      }
    }
    unset($type_report);
  }

  // Query for all objects, ordered by created date and offset by the value
  // stored in the context.
  $offset = $context['sandbox']['offset'];
  $limit = USC_MIRC_REPORTING_BATCH_SIZE;
  list($access, $mezzanine, $vro) = $models;
  $query = <<<EOQ
SELECT ?object ?date
WHERE {
  {?object <fedora-model:hasModel> <info:fedora/$vro>}
  UNION
  {?object <fedora-model:hasModel> <info:fedora/$mezzanine>}
  UNION
  {?object <fedora-model:hasModel> <info:fedora/$access>}
  ?object <fedora-model:createdDate> ?date ;
          <fedora-view:disseminates> [<fedora-view:disseminationType> <info:fedora/*/PBCORE>]
}
ORDER BY ?date
OFFSET $offset
LIMIT $limit
EOQ;

  $tuque = islandora_get_tuque_connection();
  $results = $tuque->repository->ri->sparqlQuery($query);

  $type_report = &$context['results']['report']['types'];
  foreach ($results as $result) {
    $object = islandora_object_load($result['object']['value']);

    $intersected_models = array_intersect($models, $object->models);
    if (!empty($intersected_models)) {
      if (count($intersected_models) > 1) {
        watchdog('usc_mirc_reporting', 'Somehow have multiple video models on @pid: @models. Skipping in report.', array(
          '@pid' => $object->id,
          '@models' => implode(', ', $intersected_models),
        ));
        continue;
      }

      // Should have exactly one model... Let's get it!
      $type = reset($intersected_models);


        // Conditionally add to the date groups.
      $created_date = strtotime($result['date']['value']);
      foreach ($intervals as $interval) {
	if ($created_date <= $interval) {
	  $type_report[$interval][$type]['count']++;

	  // Get all the sizes (PBCore description doc could contain multiple).
	  $sizes = usc_mirc_reporting_get_sizes_from_pbcore($object['PBCORE']);
	  foreach ($sizes as $size_from_pbcore) {
	    // Parse once... Should be a little more efficient when adding again
	    // later?
	    $size = gmp_init($size_from_pbcore, 10);

            $type_report[$interval][$type]['size'] = gmp_add($type_report[$interval][$type]['size'], $size);
          }
        }
      }
    }
    else {
      watchdog('usc_mirc_reporting', 'You found an anomaly!... @pid was somehow selected as having a content model, but then when we checked the object, we couldn\'t find any we were interested in...', array(
        '@pid' => $object->id,
      ));
    }
  }

  if (count($results) < USC_MIRC_REPORTING_BATCH_SIZE) {
    // If we didn't get a full set of results, we're finished.
    $context['finished'] = 1;

    foreach ($type_report as $interval => &$report) {
      foreach ($models as $model) {
        $report['total']['size'] = gmp_add($report['total']['size'], $report[$model]['size']);
        $report['total']['count'] += $report[$model]['count'];
      }
    }
    unset($report);
  }
  else {
    // Otherwise, keep on trucking (we'll increment the offset next time
    // around).
    $context['finished'] = 0;
  }

  $context['message'] = t('Got sizes from %offset to %end.', array(
    '%offset' => $context['sandbox']['offset'],
    '%end' => ($context['sandbox']['offset'] + count($results)),
  ));

  // Serialize the large numbers, so we can can use them next time.
  foreach ($type_report as $interval => &$report) {
    foreach ($type_to_iterate as $type) {
      $report[$type]['size'] = gmp_strval($report[$type]['size'], 10);
    }
  }
  unset($report);
}

/**
 * Get sizes from any instantiations present in the given datastream.
 *
 * @param AbstractDatastream $pbcore_datastream
 *   A datastream we expect to contain PBCore XML.
 *
 * @return array
 *   An array of strings, each of which represents the size of an
 *  instantiation from the given PBCore datastream.
 */
function usc_mirc_reporting_get_sizes_from_pbcore(AbstractDatastream $pbcore_datastream) {
  $to_return = array();

  $pbcore_simplexml = new SimpleXMLElement($pbcore_datastream->content);
  if ($pbcore_simplexml) {
    $pbcore_simplexml->registerXPathNamespace('pb', 'http://www.pbcore.org/PBCore/PBCoreNamespace.html');
    $file_size_xpath = '//pb:instantiationFileSize[@unitsOfMeasure="bytes"]';
    foreach ((array) $pbcore_simplexml->xpath($file_size_xpath) as $size) {
      $size = (string) $size;
      $size = trim($size);
      if (!empty($size)) {
        $to_return[] = $size;
      }
    }
  }
  else {
    watchdog('usc_mirc_reporting', 'Failed to load PBCORE from @pid, when looking for file sizes.', array(
      '@pid' => $pbcore_datastream->parent->id,
    ));
  }

  return $to_return;
}

/**
 * Batch operation to report on the number of unique titles.
 *
 * @param mixed $context
 *   An array (or ArrayObject) of batch context.
 */
function usc_mirc_reporting_batch_title_counts($intervals, $models, &$context) {
  list($access, $mezzanine, $vro) = $models;
  $relationship = 'isDerivativeOf';
  $date_filters = array();
  foreach ($intervals as $interval) {
    $date_filters[$interval] = format_string('(bound(?access) && ?access_date <= "!created_date"^^<xsd:dateTime>) || (!bound(?access) && ?vro_date <= "!created_date"^^<xsd:dateTime>)', array(
      '!created_date' => gmdate('Y-m-d\TH:i:s\Z', $interval),
    ));
  }
  $access_filters = array(
    'accessible' => 'bound(?access) && !bound(?policy_datastream)',
    'in_progress' => '!bound(?access) || bound(?policy_datastream)',
  );

  $tuque = islandora_get_tuque_connection();
  foreach ($date_filters as $interval => $date_filter) {
    $interval_report = &$context['results']['report']['titles'][$interval];

    foreach ($access_filters as $type => $access_filter) {
      $filter = format_string('FILTER((!date_filter) && (!access_filter))', array(
        '!date_filter' => $date_filter,
        '!access_filter' => $access_filter,
      ));

      $query = <<<EOQ
PREFIX islandora-rels-ext: <http://islandora.ca/ontology/relsext#>
SELECT ?object
WHERE {
  ?object <fedora-model:hasModel> <info:fedora/$vro> ;
          <fedora-model:createdDate> ?vro_date .
  OPTIONAL {
    ?mezz <fedora-rels-ext:$relationship> ?object ;
	  <fedora-model:hasModel> <info:fedora/$mezzanine> .
    ?access <fedora-rels-ext:$relationship> ?mezz ;
	    <fedora-view:lastModifiedDate> ?access_date .
    OPTIONAL{
      ?access <fedora-view:disseminates> ?policy_datastream .
      ?policy_datastream <fedora-view:disseminationType> <info:fedora/*/POLICY>
    }
  }
  $filter
}
EOQ;
      // XXX: Would probably be better done with a "count" query; however,
      // they're not supported by Tuque, as of writing this.
      $results = $tuque->repository->ri->sparqlQuery($query);
      $interval_report[$type] = count($results);
    }

    // Unset, just in this is made to iterate more.
    unset($interval_report['total']);
    $interval_report['total'] = array_sum($interval_report);
  }
}
