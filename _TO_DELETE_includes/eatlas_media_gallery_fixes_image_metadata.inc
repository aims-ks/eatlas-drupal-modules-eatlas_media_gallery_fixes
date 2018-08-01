<?php

// NOTE: PHP don't currently define GPSHPositioningError tag (PHP5 v5.3.10)
// The following line will define it ONLY if it's not already defined.
defined('GPSHPositioningError') or define('GPSHPositioningError', 'UndefinedTag:0x001F');


/**
 * Implements hook_form_FORM_ID_alter().
 * Add a fields to the "Add file" window, in the media browser module.
 * See: modules/media_browser_plus/media_browser_plus.module
 *
 * Field: Submitter
 * Field: Projects
 *
 * IMPORTANT
 * Add the field 'field_submitter', otherwise the value won't be save.
 * Admin > Configuration > MEDIA: File types
 *     Image: manage fields
 *         Label: Submitter
 *         Machine name: field_submitter
 *         Field type: Node reference
 *         Widget: Autocomplete text field
 *         Number of values: 1
 *         Content types that can be referenced: Person
 */
function eatlas_media_gallery_fixes_form_media_add_upload_multiple_alter(&$form, &$form_state) {
	// Check if "person" entity exists before adding the field
	$personContentType = node_type_load('person');
	if ($personContentType) {
		$form['field_submitter'] = array(
			'#type' => 'textfield',
			'#autocomplete_path' => 'node_reference/autocomplete/file/image/field_submitter',
			'#language' => LANGUAGE_NONE,
			'#title' => t('Submitter'),
			'#field_name' => 'field_submitter',
			'#weigth' => -3,
			'#description' => t('Select the person that submitted the photos. If the person is not in the list, add it before submitting its photos.'),

			'#field_parents' => array(),
			'#default_value' => NULL,
			'#element_validate' => array('node_reference_autocomplete_validate'),
			'#value_callback' => 'node_reference_autocomplete_value'
		);
	}

	// Add a project field if there is some projects
	$projects = entity_load('node', FALSE, array('type' => 'project'));
	if ($projects) {
		$projectList = array('_none' => '- None -');
		foreach ($projects as $project) {
			$projectList[$project->nid] = $project->title;
		}
		asort($projectList);
		$form['field_project'] = array(
			'#type' => 'select',
			'#language' => LANGUAGE_NONE,
			'#title' => t('Projects'),
			'#field_name' => 'field_project',
			'#weigth' => -2,
			'#description' => t('If all the files are associated with one or more projects, select them from the list.'),

			'#field_parents' => array(),
			'#properties' => array(
				'strip_tags' => TRUE,
				'optgroups' => TRUE,
				'empty_option' => 'option_none',
				'filter_xss' => FALSE
			),
			'#value_key' => 'nid',
			'#default_value' => NULL,
			'#element_validate' => array('options_field_widget_validate'),
			'#multiple' => TRUE,
			'#options' => $projectList,
			'#after_build' => array('field_form_element_after_build')
		);
	}

	$form['#submit'][] = '_eatlas_media_gallery_fixes_form_media_add_upload_multiple_submit';
}

function _eatlas_media_gallery_fixes_form_media_add_upload_multiple_submit($form, &$form_state) {
	$fids = $form_state['redirect'][1]['query']['fid'];
	if (!is_array($fids)) {
		$fids = array($fids);
	}
	$media_entities = file_load_multiple($fids);

	// Apply submitter
	$submitter = $form_state['values']['field_submitter'];
	$projects = $form_state['values']['field_project'];
	if ($submitter !== NULL || $projects !== NULL) {
		foreach ($media_entities as $media) {
			if ($submitter !== NULL) {
				$media->field_submitter[LANGUAGE_NONE] = array(array('nid' => $submitter));
			}
			if ($projects !== NULL) {
				$media->field_project[LANGUAGE_NONE] = $projects;
			}
			file_save($media);
		}
	}
}

/**
 * Utility method to get a value from an image metadata.
 * $metadata: Array returned by "_eatlas_media_gallery_fixes_get_image_metadata"
 * $key: Key defined in $eatlas_media_gallery_fixes_exif_fields ('title', 'description', etc)
 */
function eatlas_media_gallery_fixes_get_metadata_value($metadata, $key) {
	// Location of values, in the metadata, in order of preference.
	// NOTE: This has to be in sync with EAtlasField.java in the ImageMetadataEditor application.
	// Declare the variable array (static = declared only once)
	static $eatlas_media_gallery_fixes_exif_fields = array(
		'title'           => array(array('WINXP', 'Title'), array('IFD0', 'ImageDescription'), array('IPTC', 'city')),
		'description'     => array(array('WINXP', 'Subject'), array('IPTC', 'caption')),
		'photographers'   => array(array('WINXP', 'Author'), array('IPTC', 'by_line')),
		'attribution'     => array(array('IPTC', 'copyright_notice')),
		// Used for Licence
		'keywords'        => array(array('WINXP', 'Keywords'), array('IPTC', 'keywords')),
		// Used for the reason to not choose Creative Commons licence
		'comments'        => array(array('WINXP', 'Comments')),

		'longitude'       => array(array('GPS', 'GPSLongitude')),
		'longitudeRef'    => array(array('GPS', 'GPSLongitudeRef')), // 'E' or 'W'
		'latitude'        => array(array('GPS', 'GPSLatitude')),
		'latitudeRef'     => array(array('GPS', 'GPSLatitudeRef')), // 'N' or 'S'

		'radius'          => array(array('GPS', GPSHPositioningError)),

		// Unused - Kept for reference
		'altitude'        => array(array('GPS', 'GPSAltitude')),
		'altitudeRef'     => array(array('GPS', 'GPSAltitudeRef')), // 0 = Above sea level, 1 = Below sea level (default 0)
		'imgDirection'    => array(array('GPS', 'GPSImgDirection')),
		'imgDirectionRef' => array(array('GPS', 'GPSImgDirectionRef')), // 'T' = True direction, 'M' = Magnetic direction

		'orientation'     => array(array('EXIF', 'Orientation'))
	);

	if (!$metadata || !$key || !array_key_exists($key, $eatlas_media_gallery_fixes_exif_fields)) {
		return NULL;
	}

	$tags = $eatlas_media_gallery_fixes_exif_fields[$key];
	foreach ($tags as $tag) {
		if (array_key_exists($tag[0], $metadata)) {
			$group = $metadata[$tag[0]];
			if ($group) {
				if ($tag[0] === 'IPTC') {
					$value = _eatlas_media_gallery_fixes_get_iptc_value($group, $tag[1]);
					if ($value) {
						return $value;
					}
				} else {
					if (array_key_exists($tag[1], $group)) {
						if ($tag[0] === 'GPS' && ($tag[1] === 'GPSLongitude' || $tag[1] === 'GPSLatitude' || $tag[1] === 'GPSAltitude' || $tag[1] === 'GPSImgDirection')) {
							return _eatlas_media_gallery_fixes_get_gps_coordinate($group[$tag[1]]);
						} else if ($tag[0] === 'GPS' && $tag[1] === GPSHPositioningError) {
							return _eatlas_media_gallery_fixes_get_gps_radius($group[$tag[1]]);
						} else {
							$value = $group[$tag[1]];
							if (is_string($value)) {
								$value = trim($value);
							}
							if ($value) {
								return $value;
							}
						}
					}
				}
			}
		}
	}

	return NULL;
}

/**
 * Extract the metadata from an image and save it in the appropriate
 * Drupal fields.
 */
function eatlas_media_gallery_fixes_set_image_metadata($imageFile) {
	$imageFileURI = drupal_realpath($imageFile->uri);
	$metadata = _eatlas_media_gallery_fixes_get_image_metadata($imageFileURI);
	if ($metadata) {
		$persons = NULL; $organisations = NULL; $organisationSections = NULL;
		// WARNING: Watchdog CRASH when the message contains invalid
		//     char like \0 and it's impossible to catch that exception.
		//     In this case, file_put_contents do a better job.
		//file_put_contents('/home/reefatlas/_TO_DELETE_/output.txt', print_r($metadata, TRUE));

		// AIMS Tags
		// X   static String TAG_LATITUDE = "GPSLatitude";
		// X   static String TAG_LONGITUDE = "GPSLongitude";
		// X   static String TAG_DEPTH = "GPSAltitude";
		//     static String TAG_THUMBNAIL = "ThumbnailImage";
		//     static String TAG_THUMBNAIL_SIZE = "ThumbnailLength";
		// X   static String TAG_KEYWORDS = "Keywords";
		// X   static String TAG_PHOTOGRAPHER = "By-line";
		// X   static String TAG_LOCATION = "City";
		// X   static String TAG_COPYRIGHT = "CopyrightNotice";
		// X   static String TAG_CAPTION = "Caption-Abstract";
		// X   static String TAG_LAT_DIR = "GPSLatitudeRef";
		// X    static String TAG_LONG_DIR = "GPSLongitudeRef";

		$gps = NULL;
		$gps_valid = TRUE;

		$lon = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'longitude');
		$lonRef = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'longitudeRef');
		if ($lonRef) {
			if (!$lonRef === 'E' && !$lonRef === 'W') {
				$gps_valid = FALSE;
			} else if ($lonRef === 'W') {
				$lon *= -1;
			}
		}

		$lat = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'latitude');
		$latRef = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'latitudeRef');
		if ($latRef) {
			if (!$latRef === 'N' && !$latRef === 'S') {
				$gps_valid = FALSE;
			} else if ($latRef === 'S') {
				$lat *= -1;
			}
		}

		$radius = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'radius');

		$title = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'title');
		$description = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'description');
		$attribution = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'attribution');
		$keywords = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'keywords');
		$photographers = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'photographers');

		$dirty = FALSE;

		if ($title) {
			_eatlas_media_gallery_fixes_set_file_values($imageFile, 'media_title', $title);
			$dirty = TRUE;
		}

		if ($description) {
			_eatlas_media_gallery_fixes_set_file_values($imageFile, 'media_description', $description);
			$dirty = TRUE;
		}

		if ($keywords) {
			$keywordsArray = explode(';', $keywords);

			$license = NULL;
			foreach ($keywordsArray as $keyword) {
				$keyword = trim($keyword);
				// The user may had enter "CC_BY", "cc by", "CC - BY", etc.
				// Even it the user really try to write it as "CC-BY",
				// the edition software (like Ms Excel) may decide to
				// replace the hyphen "-" with some weird char that looks
				// just like it (there is a huge collection of them in
				// UTF-8).
				// An easy solution is to remove all non Alpha numeric
				// chars.
				// NOTE: strcasecmp: Binary safe case-insensitive string comparison 
				$safeKeyword = preg_replace("/[^\w\d]/", '', $keyword);
				// License values defined in
				//     file: media_gallery/media_gallery.fields.inc
				//     function: _media_gallery_get_field_license_values
				if (strcasecmp($safeKeyword, 'CCSANC') === 0 ||
						strcasecmp($safeKeyword, 'CCNCSA') === 0 ||
						strcasecmp($safeKeyword, 'CCBYSANC') === 0 ||
						strcasecmp($safeKeyword, 'CCBYNCSA') === 0) {
					$license = 'cc_sa_nc';

				} else if (strcasecmp($safeKeyword, 'CCNC') === 0 ||
						strcasecmp($safeKeyword, 'CCBYNC') === 0) {
					$license = 'cc_nc';

				} else if (strcasecmp($safeKeyword, 'CCNDNC') === 0 ||
						strcasecmp($safeKeyword, 'CCNCND') === 0 ||
						strcasecmp($safeKeyword, 'CCBYNDNC') === 0 ||
						strcasecmp($safeKeyword, 'CCBYNCND') === 0) {
					$license = 'cc_nd_nc';

				} else if (strcasecmp($safeKeyword, 'CC') === 0 ||
						strcasecmp($safeKeyword, 'CCBY') === 0) {
					$license = 'cc';

				} else if (strcasecmp($safeKeyword, 'CCSA') === 0 ||
						strcasecmp($safeKeyword, 'CCBYSA') === 0) {
					$license = 'cc_sa';

				} else if (strcasecmp($safeKeyword, 'CCND') === 0 ||
						strcasecmp($safeKeyword, 'CCBYND') === 0) {
					$license = 'cc_nd';

				} else if (strcasecmp($safeKeyword, 'Copyright')) {
					$license = 'copyright';
				}
				if ($license) {
					break;
				}
			}

			// 'copyright' is the default value (no need to update)
			if ($license && $license !== 'copyright') {
				_eatlas_media_gallery_fixes_set_file_values($imageFile, 'field_license', $license);
				$dirty = TRUE;
			} else {
				// It's copyright, we need to save the reason for not been CC-BY
				$comments = eatlas_media_gallery_fixes_get_metadata_value($metadata, 'comments');
				$reasonForNotCCBY = _eatlas_media_gallery_fixes_get_reason_for_not_ccby($comments);
				if ($reasonForNotCCBY) {
					_eatlas_media_gallery_fixes_set_file_values($imageFile, 'field_notes', $reasonForNotCCBY);
					$dirty = TRUE;
				}
			}
		}

		if ($gps_valid) {
			_eatlas_media_gallery_fixes_set_file_values($imageFile, 'field_gpslongitude', $lon);
			_eatlas_media_gallery_fixes_set_file_values($imageFile, 'field_gpslatitude', $lat);
			$dirty = TRUE;
		}

		if ($radius) {
			// The radius (field GPSHPositioningError) is in meters. We want a radius in km.
			$kmRadius = $radius / 1000;
			_eatlas_media_gallery_fixes_set_file_values($imageFile, 'field_gpsradius', $kmRadius);
		}

		if ($photographers) {
			// Load needed web site entities (if they haven been loaded yet)
			if ($persons === NULL) {
				$persons = entity_load('node', FALSE, array('type' => 'person'));
			}

			// Sometime $photographers is already an array... I'm not sure why
			if (!is_array($photographers)) {
				// TODO explode the String? Why this variable is an array? How can I set multiple photographers?
				$photographers = array($photographers);
			}
			// $photographers is not always splited (I never seen it splited).
			// The values looks like: "Foo Bar;John Smith"
			$photographersArray = array();
			foreach ($photographers as $photographer) {
				$photographersArray = array_merge($photographersArray, explode(';', $photographer));
			}

			$photographersEntities = _eatlas_media_gallery_fixes_get_persons_and_organisations($photographersArray, $persons, NULL, NULL);
			$matchingPhotographersIds = $photographersEntities[0];
			$freeTextPhotographers = $photographersEntities[1];

			if (count($matchingPhotographersIds)) {
				_eatlas_media_gallery_fixes_set_file_values($imageFile, 'field_photographers', $matchingPhotographersIds, 'nid');
				$dirty = TRUE;
			}
			if ($freeTextPhotographers) {
				_eatlas_media_gallery_fixes_set_file_values($imageFile, 'field_custom_photographers', $freeTextPhotographers);
				$dirty = TRUE;
			}
		}

		if ($attribution) {
			// Load needed web site entities (if they haven been loaded yet)
			if ($persons === NULL) {
				$persons = entity_load('node', FALSE, array('type' => 'person'));
			}
			if ($organisations === NULL) {
				$organisations = entity_load('node', FALSE, array('type' => 'organisation'));
			}
			if ($organisationSections === NULL) {
				$organisationSections = entity_load('node', FALSE, array('type' => 'organisation_section'));
			}

			// This field (usually an array) contains the attribution
			// which is usually a list of "Person" and/or "Institution"
			if (!is_array($attribution)) {
				$attribution = array($attribution);
			}
			$attributionArray = array();
			foreach ($attribution as $attributionItem) {
				$attributionArray = array_merge($attributionArray, explode(';', $attributionItem));
			}

			$attributionEntities = _eatlas_media_gallery_fixes_get_persons_and_organisations($attributionArray, $persons, $organisations, $organisationSections);
			$matchingAttributionIds = $attributionEntities[0];
			$freeTextAttribution = $attributionEntities[1];

			if (count($matchingAttributionIds)) {
				_eatlas_media_gallery_fixes_set_file_values($imageFile, 'field_attribution', $matchingAttributionIds, 'nid');
				$dirty = TRUE;
			}
			if ($freeTextAttribution) {
				_eatlas_media_gallery_fixes_set_file_values($imageFile, 'field_custom_attribution', $freeTextAttribution);
				$dirty = TRUE;
			}
		}

		if ($dirty) {
			// Save the Drupal image object (save the fields to the DB)
			// https://api.drupal.org/api/drupal/includes%21file.inc/function/file_save/7
			file_save($imageFile);
		}
	}
}

function _eatlas_media_gallery_fixes_get_persons_and_organisations($personsNameArray, $persons, $organisations, $organisationSections) {
	// Make 3 groups (org, persons with org, persons) and merge the 3 groups toggether at the end
	$matchingOrganisationsIds = array();
	$matchingPersonsWithOrganisationIds = array();
	$matchingPersonsIds = array();

	$notMatchingPersons = '';

	foreach ($personsNameArray as $rawPersonName) {
		$rawPersonName = trim($rawPersonName);
		$personName = NULL;
		$organisationName = NULL;
		// $personName is usually in the form of "name (organisation)". We need to extract both parts
		if (preg_match('/(.*)\((.*)\)/', $rawPersonName, $matches)) {
			$personName = trim($matches[1]);
			$organisationName = trim($matches[2]);
		}

		if ($personName && $organisationName) {
			// There is an organisation in brakets after the name
			// IE: "John Doe (AIMS)"
			$matchingPerson = _eatlas_media_gallery_fixes_get_entity_by_alias($personName, $persons);

			if ($organisations == NULL && $organisationSections == NULL) {
				// If NULL is passed for the organisations, we don't care about it (photographers)
				if ($matchingPerson !== NULL) {
					array_push($matchingPersonsIds, $matchingPerson->nid);
				} else {
					if ($notMatchingPersons) {
						$notMatchingPersons .= ', ';
					}
					$notMatchingPersons .= $rawPersonName;
				}
			} else {
				// The list of organisations is not null, look for it (attribution)
				$matchingOrganisation = NULL;
				if ($organisations) {
					$matchingOrganisation = _eatlas_media_gallery_fixes_get_entity_by_alias($organisationName, $organisations);
				}
				if ($matchingOrganisation === NULL && $organisationSections) {
					$matchingOrganisation = _eatlas_media_gallery_fixes_get_entity_by_alias($organisationName, $organisationSections);
				}

				if ($matchingPerson !== NULL && $matchingOrganisation !== NULL) {
					// Both the person AND the organisation has been found.
					array_push($matchingPersonsWithOrganisationIds, $matchingPerson->nid);
					array_push($matchingPersonsWithOrganisationIds, $matchingOrganisation->nid);
				} else {
					// At lease one is missing (either the person or the organisation)
					// Add what has been found to the list, and also add the
					// full attribution string to the free text attribution
					if ($matchingPerson !== NULL) {
						array_push($matchingPersonsIds, $matchingPerson->nid);
					} else if ($matchingOrganisation !== NULL) {
						array_push($matchingOrganisationsIds, $matchingOrganisation->nid);
					}

					if ($notMatchingPersons) {
						$notMatchingPersons .= ', ';
					}
					$notMatchingPersons .= $rawPersonName;
				}
			}
		} else {
			// There is no organisation in brakets after the name.
			// It can either be a standalong person name or an organisation.
			// IE: "AIMS", "John Doe", etc.
			$matchingPerson = _eatlas_media_gallery_fixes_get_entity_by_alias($rawPersonName, $persons);
			$matchingOrganisation = NULL;
			if ($matchingPerson === NULL && $matchingOrganisation == NULL && $organisations) {
				$matchingOrganisation = _eatlas_media_gallery_fixes_get_entity_by_alias($rawPersonName, $organisations);
			}
			if ($matchingPerson === NULL && $matchingOrganisation == NULL && $organisationSections) {
				$matchingOrganisation = _eatlas_media_gallery_fixes_get_entity_by_alias($rawPersonName, $organisationSections);
			}

			// If the person / organisation is not found, it goes to the "Free text attributions"
			if ($matchingPerson !== NULL) {
				array_push($matchingPersonsIds, $matchingPerson->nid);
			} else if ($matchingOrganisation !== NULL) {
				array_push($matchingOrganisationsIds, $matchingOrganisation->nid);
			} else {
				if ($notMatchingPersons) {
					$notMatchingPersons .= ', ';
				}
				$notMatchingPersons .= $rawPersonName;
			}
		}
	}

	return array(array_merge($matchingOrganisationsIds, $matchingPersonsWithOrganisationIds, $matchingPersonsIds), $notMatchingPersons);
}

function _eatlas_media_gallery_fixes_get_entity_by_alias($entityName, $entities) {
	if ($entities !== NULL) {
		foreach ($entities as $entity) {
			if (isset($entity->title) && $entity->title && strcasecmp($entity->title, $entityName) === 0) {
				return $entity;
			}
			if (isset($entity->field_aliases) && $entity->field_aliases && $entity->field_aliases[LANGUAGE_NONE]) {
				foreach ($entity->field_aliases[LANGUAGE_NONE] as $aliasArray) {
					if ($aliasArray && $aliasArray['value']) {
						$alias = $aliasArray['value'];
						if (strcasecmp($alias, $entityName) === 0) {
							return $entity;
						}
					}
				}
			}
		}
	}
	return NULL;
}

/**
 * Wrapper around the PHP functions (exif_read_data and GetImageSize)
 * used to extract an image metadata.
 */
function _eatlas_media_gallery_fixes_get_image_metadata($imageURI) {
	if (!file_exists($imageURI)) {
		return array();
	}

	// Set EXIF encoding to UTF-8, globally;
	// When the system tries to read some groups (like WINXP) with
	// the default encoding "ISO-8859-15", it crashes the system
	// while trying to update the database.
	ini_set('exif.encode_unicode', 'UTF-8');

	$metadata = exif_read_data($imageURI, 'ANY_TAG', TRUE);
	if (!$metadata) {
		$metadata = array();
	}

	// exif_read_data can't read IPTC
	$size = GetImageSize($imageURI, $infoImage);
	if (isset($infoImage["APP13"])) {
		$iptc = iptcparse($infoImage["APP13"]);
		$metadata['IPTC'] = $iptc;
	}

	return $metadata;
}

/**
 * The GPS values (longitude and latitude) are saved as 3 rational
 * numbers in the EXIF metadata. They need to be added up, with their
 * respective proportions.
 * IE: Degree + minutes/60 + secondes/3600
 */
function _eatlas_media_gallery_fixes_get_gps_coordinate($gpsCoordinateArray) {
	if (!is_array($gpsCoordinateArray)) {
		$gpsCoordinateArray = array($gpsCoordinateArray);
	}
	$dec = 0;
	$granularity = 0;
	foreach ($gpsCoordinateArray as $element) {
		$parts = explode('/', $element);
		if (!$parts[0] || !$parts[1]) {
			$dec += 0;
		} else {
			$dec += (float) (((float) $parts[0] /  (float) $parts[1]) / pow(60, $granularity));
		}
		$granularity++;
	}
	return $dec;
}
function _eatlas_media_gallery_fixes_get_gps_radius($gpsRawRadius) {
	$parts = explode('/', $gpsRawRadius);
	if (!$parts[0] || !$parts[1]) {
		return 0;
	} else {
		return (float) ((float) $parts[0] /  (float) $parts[1]);
	}
}

function _eatlas_media_gallery_fixes_get_reason_for_not_ccby($comments) {
	$REASON_PREFIXES = array(
		"Licence extra: ", // Licence is spelt with a "c" in Australia and UK
		"License extra: "  // License is spelt with a "s" in USA
	);

	$lines = preg_split('/\R/', $comments);
	foreach ($lines as $line) {
		foreach ($REASON_PREFIXES as $prefix) {
			if (strpos($line, $prefix) === 0) {
				return substr($line, strlen($prefix));
			}
		}
	}
	return NULL;
}

/**
 * Return the IPTC value for a given human readable key.
 */
function _eatlas_media_gallery_fixes_get_iptc_value($iptc, $key) {
	$keyCode = _eatlas_media_gallery_fixes_get_iptc_key_code($key);
	if ($keyCode === NULL || !isset($iptc[$keyCode])) {
		return NULL;
	}
	$value = $iptc[$keyCode];
	if ($value) {
		// '\x1B%G' = UTF-8
		// '\x1b(B' = ASCII
		// '\x1b-A', '\x1b(@\x1b-A' or '\x1b(B\x1b-A' = iso-8859-1
		// etc.
		// See: https://github.com/brion/MediaWiki/blob/master/includes/media/IPTC.php
		$isUTF8 = isset($iptc[_eatlas_media_gallery_fixes_get_iptc_key_code('envelope_character_set')]) ?
				$iptc[_eatlas_media_gallery_fixes_get_iptc_key_code('envelope_character_set')] === '\x1B%G' :
				FALSE;
		if (is_string($value)) {
			if (!$isUTF8) {
				$value = utf8_encode($value);
			}
			$value = trim($value);
		} else if (is_array($value)) {
			foreach ($value as $key => $val) {
				if (!$isUTF8) {
					$val = utf8_encode($val);
				}
				$value[$key] = trim($val);
			}
		}
	}

	return $value;
}

/**
 * Return the IPTC computer code for a given human readable key.
 */
function _eatlas_media_gallery_fixes_get_iptc_key_code($key) {
	$keyCodesMap = array (
		'object_data_preview_data' => '2#202',
		'object_data_preview_file_format_version' => '2#201',
		'object_data_preview_file_format' => '2#200',
		'audio_outcue' => '2#154',
		'audio_duration' => '2#153',
		'audio_sampling_resolution' => '2#152',
		'audio_sampling_rate' => '2#151',
		'audio_type' => '2#150',
		'language_identifier' => '2#135',
		'image_orientation' => '2#131',
		'image_type' => '2#130',
		'rasterized_caption' => '2#125',    
		'writer' => '2#122',
		'caption' => '2#120',
		'contact' => '2#118',
		'copyright_notice' => '2#116',
		'source' => '2#115',
		'credit' => '2#110',
		'headline' => '2#105',
		'original_transmission_reference' => '2#103',
		'country_name' => '2#101',
		'country_code' => '2#100',
		'state' => '2#095',
		'sublocation' => '2#092',
		'city' => '2#090',
		'by_line_title' => '2#085',
		'by_line' => '2#080',
		'object_cycle' => '2#075',
		'program_version' => '2#070',
		'originating_program' => '2#065',
		'digital_creation_time' => '2#063',
		'digital_creation_date' => '2#062',   
		'creation_time' => '2#060',
		'creation_date' => '2#055',
		'reference_number' => '2#050',
		'reference_date' => '2#047',
		'reference_service' => '2#045',
		'action_advised' => '2#042',
		'special_instruction' => '2#040',
		'expiration_time' => '2#038',
		'expiration_date' => '2#037',
		'release_time' => '2#035',
		'release_date' => '2#030',
		'content_location_name' => '2#027',
		'content_location_code' => '2#026',
		'keywords' => '2#025',
		'fixture_identifier' => '2#022',
		'supplemental_category' => '2#020', 
		'category' => '2#015',
		'subject_reference' => '2#010', 
		'urgency' => '2#010',
		'editorial_update' => '2#008',
		'edit_status' => '2#007',
		'object_name' => '2#005',
		'object_attribute_reference' => '2#004',
		'object_type_reference' => '2#003',
		'record_version' => '2#000',
		'envelope_character_set' => '1#090'
	);

	return isset($keyCodesMap[$key]) ? $keyCodesMap[$key] : NULL;
}

?>
