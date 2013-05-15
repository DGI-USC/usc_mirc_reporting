<?php

/**
 * @file
 * Miscellaneous batch-related functions.
 */

/**
 * Setup the batch to calculate file sizes and the number of title.
 *
 * @param callable $finished
 *   A callback for when the batch finishes. The results['report'] key should
 *   by used to the finished callback should be used to generate output.
 */
function usc_mirc_reporting_report_batch($finished) {
  $intervals = array();
  $now = time();
  $intervals[] = strtotime('30 days ago', $now);
  $intervals[] = strtotime('1 year ago', $now);


  // Get the content model URIs we use.
  $models = array();
  $models[] = variable_get('usc_mirc_cmodel_access', 'usc:accessCModel');
  $models[] = variable_get('usc_mirc_cmodel_mezzanine', 'usc:mezzanineCModel');
  $models[] = variable_get('usc_mirc_cmodel_vro', 'usc:vroCModel');

  $batch = array(
    'title' => 'USC MIRC Report Generation',
    'finished' => $finished,
    'operations' => array(
      array('usc_mirc_reporting_batch_file_sizes_and_type_count', array($intervals, $models)),
      array('usc_mirc_reporting_batch_title_counts', array($intervals)),
    ),
  );
}

define('USC_MIRC_REPORTING_BATCH_SIZE', 100);

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
 *   - type: A string representing the type, one of those included in the
 *     $models parameter or the string "total". In turn, it contains:
 *     - time: Either the string "all_time" or an integer denoting the
 *       beginning of an interval. Contains:
 *       - "size": A string containing the aggregated size.
 *       - "count": An integer containing the count of items.
 */
function usc_mirc_reporting_batch_file_sizes_and_type_count($intervals, $models, &$context) {
  // Types to iterate... We keep track of the "total" here, in addition to the
  // the models themselves.
  $type_to_iterate = array('total');

  // Add the models to the types to iterate over (when (un)serializing).
  $type_to_iterate += $models;

  $intervals_to_iterate = array('all_time') + $intervals;

  if (!isset($context['sandbox']['offset'])) {
    // Some initialization.
    $context['sandbox']['offset'] = 0;
    foreach ($type_to_iterate as $type) {
      foreach ($intervals_to_iterate as $interval) {
        $context['results']['report']['types'][$type][$interval] = array(
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
    foreach ($type_to_iterate as $type) {
      $type_report = &$context['results']['report']['types'][$type];
      foreach ($intervals_to_iterate as $interval) {
        $type_report[$interval]['size'] = gmp_init($type_report[$interval]['size'], 10);
      }
    }
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

  $tuque = islandora_get_tuque_connection():
  $results = $tuque->repository->ri->sparqlQuery($query);

  $type_report = &$context['results']['report']['types'];
  foreach ($results as $result) {
    $object = islandora_object_load($result['object']['value']);

    $intersected_models = array_intersect($models, $object->models);
    if (!empty($intersected_models) {
      if (count($intersected_models) > 1) {
        watchdog('usc_mirc_reporting', 'Somehow have multiple models on @pid: @models. Skipping in report.', array(
          '@pid' => $object->id,
          '@models' => implode(', ', $intersected_models),
        ));
        continue;
      }

      // Should have exactly one model... Let's get it!
      $type = reset($intersected_models);

      // Increment the count.
      $type_report[$type]['all_time']['count']++;

      // Get all the sizes (PBCore description doc could contain multiple).
      $sizes = usc_mirc_reporting_get_size_from_pbcore($object['PBCORE']);
      foreach ($sizes as $size) {
        // Parse once... Should be a little more efficient when adding again
        // later?
        $size = gmp_init($size, 10);

        // Add to the total.
        $type_report[$type]['all_time']['size'] = gmp_add($type_report[$type]['all_time']['size'], $size);

        // Conditionally add to the date groups.
        $created_date = strtotime($result['date']['value']);
        foreach ($intervals as $interval) {
          if ($created_date > $interval) {
            $type_report[$type][$interval]['size'] = gmp_add($type_report[$type][$interval]['size'], $size);
            $type_report[$type][$interval]['count']++;
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

    // Accumulate our result into the "totals".
    foreach ($models as $model) {
      foreach ($intervals_to_iterate as $interval) {
        $type_report['total'][$interval]['size'] = gmp_add($type_report['total'][$interval]['size'], $type_report[$model][$interval]['size']);
        $type_report['total'][$interval]['count'] += $type_report[$model][$interval]['count'];
      }
    }
  }
  else {
    // Otherwise, keep on trucking (we'll increment the offset next time
    // around).
    $context['finished'] = 0;
  }

  // Serialize the large numbers, so we can can use them next time.
  foreach ($type_to_iterate as $type) {
    foreach ($intervals_to_iterate as $interval) {
      $type_report[$type][$interval]['size'] = gmp_strval($type_report[$type][$interval]['size'], 10);
    }
  }
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
    foreach((array)$pbcore_simplexml->xpath($file_size_xpath) as $size) {
      $size = (string)$size;
      $to_return[] = trim($size);
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
function usc_mirc_reporting_batch_title_counts(&$context) {

}
