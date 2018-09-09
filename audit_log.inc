<?php

// $Id$
/**
 * @file
 * Audit log functions.
 *
 * DEBUG OPTIONS
 *
 * $sql = "SELECT search_json FROM {drupal_audit_log} WHERE id = 1";
 * $result = db_fetch_object(db_query($sql));
 * print_r(gzinflate(base64_decode($result->search_json)));exit;
 *
 */
// Including external libraries / modules
module_load_include('inc', 'learninglab', 'inc/eureka');

/**
 * Interface to log events into audit log.
 */
interface IAuditLogEvent {

    public function is_valid();

    public function save_event();

    public function save_event_file();
}

/**
 * Class to register audit log.
 * This must track the pages when the user has filled up HCP Form.
 */
class AuditLog {

    private $user_track_id;

    /*
     * The way to get a new instance of Audit Log is calling get_instance method.
     */

    private function __construct() {
        
    }

    static function get_instance() {
        return new AuditLog();
    }

    /*
     * Check mobile device
     */

    private function is_mobile_device() {
        return get_device_context();
    }

    /*
     * Check if the user has filled up HCP Form already.
     */

    private function is_user_logged_in() {
        if (isset($_SESSION["InquiryID"]) && is_numeric($_SESSION["InquiryID"])) {
            $this->user_track_id = $_SESSION["InquiryID"];
            return TRUE;
        }
        return FALSE;
    }

    /**
     * Function to log events, except AJAX calls, when the user is logged.
     * @param String $search_zip ZIP return by GSA or already cached.
     * @param Object/Boolean(false) $search_obj
     * @param Boolen(FALSE) control if event should ignore the time of the last one and force re-log
     */
    public function register_event($search_zip = FALSE, $search_obj = FALSE, $log_again = 0) {
        // Getting device (desktop or mobile)
        $device = self::is_mobile_device();

        //Ignore AJAX calls.
        if (
            ( isset($_POST) && is_array($_POST) && (count($_POST) > 0) ) ||
            ( isset($_SERVER['HTTP_X_REQUESTED_WITH']) && !empty($_SERVER['HTTP_X_REQUESTED_WITH']) ) ||
            ( isset($_SERVER['HTTP_X_REQUESTED_WITH']) && strtolower($_SERVER['HTTP_X_REQUESTED_WITH']) == 'xmlhttprequest')
        ) {
            return FALSE;
        }

        if (!$this->is_user_logged_in()) {
            return FALSE;
        }

        if ($search_zip === FALSE) {
            $event = new NavigationLog($this, $device);
        }
        else {
            $event = new SearchLog($this, $search_zip, $search_obj, $log_again, $device); // passar se e mobile
        }


        if (isset($_SERVER['HTTP_REFERER']) && strpos($_SERVER['HTTP_REFERER'], '/printpdf/')) {

            if (strpos($_SERVER['HTTP_REFERER'], request_uri())) {
                watchdog('AuditL', 'Blocking duplicated result.');
                return;
            }
        }

        if ($event->is_valid()) {

            if ($_SESSION['dont_track_audit_log'] === TRUE) {
                $_SESSION['dont_track_audit_log'] = FALSE;
                return;
            }

            $event->save_event();
            $event->save_event_file();

            /**
             * Call class to generate the files
             * [GET BACK]
             */
            Audit_Log_Mailer_Generate_Data::get_data_to_send('FILE_CREATION');
        }
    }

    /**
     *
     * @return user track id
     */
    public function get_user_track_id() {
        return $this->user_track_id;
    }

    /**
     * Saves the term and product used on the search
     *
     * @param type $search_term
     * @param type $product
     * @return type
     */
    static function save_search_term($search_term, $product, $token) {

        $_SESSION['SEARCH_HISTORY'][$token] = array(
          'search_term' => $search_term,
          'product' => $product,
        );

        $_SESSION['SEARCH_HISTORY']['last_search'] = array(
          'token' => $token,
          'search_term' => $search_term,
          'product' => $product,
        );

        $_SESSION['SEARCH_REFERER'] = $_SERVER['SERVER_NAME'] . $_SERVER['REQUEST_URI'];
    }

    /**
     * Returns the search array with term and product recorded on the token
     *
     *
     * @param type $token
     * @return boolean
     */
    static function get_search_term($token) {

        if (!isset($_SESSION['SEARCH_HISTORY'][$token]) || $token == FALSE) {
            AuditLog::token_watchdog_debug('Token entry not found on session: (' . $token . ') SearchHistory (' .
                count($_SESSION['SEARCH_HISTORY']) . ') ' . var_export($_SESSION['SEARCH_HISTORY'], true));
            return FALSE;
        }

        return $_SESSION['SEARCH_HISTORY'][$token];
    }

    /**
     * Gets the last search term used
     *
     * @return boolean
     */
    static function get_last_search_term() {
        if (!isset($_SESSION['SEARCH_HISTORY']['last_search'])) {
            return FALSE;
        }

        return $_SESSION['SEARCH_HISTORY']['last_search'];
    }

    /**
     * Generates a random token
     *
     * @return type
     */
    static function generate_token() {
        return uniqid(rand(), true);
    }

    /**
     * Gets the token from the URL
     *
     * @param type $url
     * @return Token if found it, otherwise, FALSE
     */
    static function get_token_from_url($url) {
        $args = explode('/', $url);
        $tidpos = strpos($args[sizeof($args) - 1], 'tid');

        if (!$tidpos) {
            return FALSE;
        }

        $tidquery = explode('&', substr($args[sizeof($args) - 1], $tidpos));
        $tidquery = $tidquery[0];
        $tidquery = explode('=', $tidquery);

        return $tidquery[1];
    }

    static function token_watchdog_debug($msg) {
        if (variable_get('audit_log_url_token_debug', FALSE)) {
            watchdog('AuditLogToken', $msg);
        }
    }

}

/**
 * Class responsible to prepare GSA's XML to be stored into the database.
 */
class SearchLog implements IAuditLogEvent {

    private $audit_obj;
    private $search_info = array();
    private $search_result;
    private $search_obj;
    private $log_again;
    private $device;
    private $date;

    public function __construct($audit_obj, $search_zip, $search_obj, $log_again, $device) {
        $this->audit_obj = $audit_obj;
        $this->search_result = $search_zip;
        $this->search_obj = $search_obj;
        $this->log_again = $log_again;
        $this->device = $device;
        // Set date
        $this->date = date('Y-m-d G:i:s');
    }

    /*
     * Check if the event is valid.
     * Only the user isn't navigating thru
     * search pagination.
     * @return boolean(true) if empty.
     */

    public function is_valid() {

        $minimum_track_time = intval(variable_get('audit_log_minimum_identical_log_time', 4));

        $result = db_query("
        SELECT * FROM
          {drupal_audit_log}
        WHERE
          user_track_id = :user_track_id AND
          query_term = :query_term AND
          product = :product AND
          searchdate >= DATE_SUB(now(), INTERVAL " . $minimum_track_time . " HOUR) AND
          log_again = :log_again", array(
          ':user_track_id' => $this->audit_obj->get_user_track_id(),
          ':query_term' => $this->search_obj->get_original_query_term(),
          ':product' => $this->search_obj->get_product(),
          ':log_again' => $this->log_again,
            )
        );

        if ($result->rowCount() > 0) {
            return FALSE;
        }

        return TRUE;
    }

    public function save_event() {
        global $base_url;
        AuditLog::token_watchdog_debug($this->search_obj->get_original_query_term() . $this->search_obj->get_product());

        /* Revision ID finding Code START */
        $xml = gzinflate($this->search_result);
        $doc = simplexml_load_string($xml);
        $version_no_arr = array();
        $sp_id_arr = array();
        $source_arr = array();
        #watchdog("Audit Log","Entered".var_export($xml, TRUE));
        foreach ($doc->RES->R as $key => $result) {
            $url_searched_arr = explode('?', $result->U);
            $url_searched = $url_searched_arr[0];
            $full_url = $base_url . '/';
            $canonical_url_val = str_ireplace($full_url, "", $url_searched);
            #watchdog("Audit Log","Event Canonical_Url_Val: ".$canonical_url_val);
            $vv_info = get_vv_info($canonical_url_val);
            #watchdog("Audit Log","Event VV_Info".var_export($vv_info, TRUE));
            $version_no_arr[] = $vv_info['version'];
            $sp_id_arr[] = $vv_info['sp_id'];
            $source_arr[] = $vv_info['source'];
        }
        #watchdog("OBJ",var_export($url_searched,TRUE));
        #watchdog("OBJ",var_export($vid_arr,TRUE));
        /* Revision ID finding Code END */
        $nid = db_insert('drupal_audit_log') // Table name no longer needs {}
            ->fields(array(
              'searchdate' => $this->date,
              'user_track_id' => $this->audit_obj->get_user_track_id(),
              'search_json' => $this->search_result,
              'query_term' => $this->search_obj->get_original_query_term(),
              'product' => $this->search_obj->get_product(),
              'log_again' => $this->log_again,
              'email_sent' => 0,
              'search_resuts_file_created' => 1,
              'device' => $this->device,
              'version_no' => implode(",", $version_no_arr),
              'sp_id' => implode(",", $sp_id_arr),
              'source' => implode(",", $source_arr),
            ))
            ->execute();
    }

    // Function to save the event in the file.
    public function save_event_file() {
        global $base_url;
        $info = new stdClass();
        /* Revision ID finding Code START */
        $xml = gzinflate($this->search_result);
        $doc = simplexml_load_string($xml);
        $version_no_arr = array();
        $sp_id_arr = array();
        $source_arr = array();
        #watchdog("Audit Log","Entered".var_export($xml, TRUE));
        foreach ($doc->RES->R as $key => $result) {
            $url_searched_arr = explode('?', $result->U);
            $url_searched = $url_searched_arr[0];
            $full_url = $base_url . '/';
            $canonical_url_val = str_ireplace($full_url, "", $url_searched);
            #watchdog("Audit Log","File Canonical_Url_Val: ".$canonical_url_val);
            $vv_info = get_vv_info($canonical_url_val);
            #watchdog("Audit Log","File VV_Info".var_export($vv_info, TRUE));
            $version_no_arr[] = $vv_info['version'];
            $sp_id_arr[] = $vv_info['sp_id'];
            $source_arr[] = $vv_info['source'];
        }
        $info->searchdate = $this->date;
        $info->user_track_id = $this->audit_obj->get_user_track_id();
        $info->search_json = $this->search_result;
        $info->query_term = $this->search_obj->get_original_query_term();
        $info->product = $this->search_obj->get_product();
        $info->log_again = $this->log_again;
        $info->email_sent = 0;
        $info->search_resuts_file_created = 1;
        $info->device = $this->device;
        //Added for VV
        $info->version_no = implode(",", $version_no_arr);
        $info->sp_id = implode(",", $sp_id_arr);
        $info->source = implode(",", $source_arr);

        _learninglab_eureka_transaction($info, 'search_json');
    }

}

/**
 * Class responsible to prepare Search's Log to be stored into the database.
 */
class NavigationLog implements IAuditLogEvent {

    private $node;
    private $page_info = '';
    private $audit_obj;
    private $track_types;
    private $product = '';
    private $device;
    private $query_term = NULL;
    private $date;

    public function __construct($audit_obj, $device) {

        $url_prefix = arg(0);
        if ($url_prefix == EXTERNAL_URL_PREFIX) {
            // get object for external search results ( JMI Integradtion )
            $object_type = 'sresult';
            $object_position = 2;
        }
        else {
            // normal search result or content
            $object_type = 'node';
            $object_position = 1;
        }

        $this->audit_obj = $audit_obj;
        if ($url_prefix == "printpdf") {
            $alias = substr($_GET['q'], 9);
            $path = drupal_lookup_path('source', $alias);
            $this->node = menu_get_object($object_type, $object_position, $path);
        }
        else {
            $this->node = menu_get_object($object_type, $object_position);
        }
        $this->track_types = array();
        $this->track_types[] = SHAREPOINT_CONTENT_TYPE_PAGES;
        $this->track_types[] = PAGE_TYPE_PAGES;
        $this->track_types[] = EXTERNAL_SEARCH_RESULT_CONTENT_TYPE;
        $this->track_types[] = ANNOUNCEMENT_CONTENT_TYPE_PAGES;
        $this->device = $device;
        // Set date
        $this->date = date('Y-m-d G:i:s');

        $this->generate_event_content();
    }

    /*
     * Function to verify if the page is a dita topic.
     */

    private function is_dita_topic() {
        $current_page = $this->node->type;
        if ($current_page == SHAREPOINT_CONTENT_TYPE_PAGES) {
            return TRUE;
        }
        else {
            return FALSE;
        }
    }

    /*
     * Function to generate page information to save into the Audit Log.
     */

    private function generate_event_content() {
        $info = array();

        if (variable_get('learning_lab_cat_subcat_debug', FALSE)) {
            watchdog('cat_subcat', var_export($this->node, TRUE));
        }

        if (is_object($this->node)) {
            if (in_array($this->node->type, $this->track_types)) {
                $debug = variable_get('SEARCH_TOKEN_DEBUG', FALSE);

                $base_path = trim(url('<front>', array('absolute' => TRUE)), '/');

                $info['accessed_url'] = $base_path . drupal_get_path_alias(request_uri());
                $info['url_referer'] = _learninglab_get_referer();

                if (empty($info['url_referer'])) {
                    //$info['url_referer'] = $_SESSION['SEARCH_REFERER'];
                    unset($_SESSION['SEARCH_REFERER']);
                }

                $tid = AuditLog::get_token_from_url(request_uri());

                // Save query term only if access bacame search results
                if ((strpos($info['accessed_url'], 'disclaimer') !== FALSE) && (strpos($info['url_referer'], '/search/solr/') !== FALSE)) {
                    $product_term = $this->audit_obj->get_last_search_term();
                    $this->query_term = $product_term['search_term'];
                    $this->product = $product_term['product'];

                    AuditLog::token_watchdog_debug($product_term['token'] . '>>>' . $this->query_term . ':' . $this->product);
                }

                // Check if it was an internal link
                $referer_llab = (strpos(AuditLogQueue::cleanup_url($info['url_referer']), AuditLogQueue::cleanup_url(variable_get('jmd_server_name', 'Projmd'))) !== FALSE);
                if ($referer_llab) {
                    $product_term = $this->audit_obj->get_search_term($tid);
                    $this->query_term = $product_term['search_term'];
                    $this->product = $product_term['product'];

                    AuditLog::token_watchdog_debug($tid . '>' . $this->query_term . ':' . $this->product);
                }

                if ($this->is_dita_topic()) {
                    // JMD Dita Topics
                    $content_inf = json_decode($this->node->field_package_info[LANGUAGE_NONE][0]['value'], TRUE);
                    $info['content'] = implode(',', $content_inf['topic']);
                    $info['references'] = $this->prepare_package_info($content_inf['references']);

                    //Get category and subcategory from node
                    if (isset($this->node->field_category[LANGUAGE_NONE][0]['value'])) {
                        $info['category'] = $this->node->field_category[LANGUAGE_NONE][0]['value'];
                    }
                    if (isset($this->node->field_subcategory[LANGUAGE_NONE][0]['value'])) {
                        $info['subcategory'] = $this->node->field_subcategory[LANGUAGE_NONE][0]['value'];
                    }

                    $this->product = $this->node->field_product[LANGUAGE_NONE][0]['value'];
                }
                else {
                    if ($this->node->type == EXTERNAL_SEARCH_RESULT_CONTENT_TYPE) {
                        // JMI Integration Documents
                        $info['content'] = '';
                        $info['accessed_url'] = $this->node->file_url; // external file URL
                        $info['access_searched'] = TRUE;
                        $info['external_content'] = TRUE;

                        //Get category and subcategory from EXTERNAL
                        $info['category'] = $this->node->field_category[LANGUAGE_NONE][0]['value'];
                        $info['subcategory'] = $this->node->field_subcategory[LANGUAGE_NONE][0]['value'];

                        $this->product = $this->node->field_product[LANGUAGE_NONE][0]['value'];
                    }
                    else {
                        // JMD Web Content
                        $info['content'] = $this->node->nid . ',' . $this->node->vid;

                        $node_loaded = node_load(arg(1));

                        if (!empty($node_loaded->field_product[LANGUAGE_NONE][0]['value']) && $node_loaded->type == SHAREPOINT_CONTENT_TYPE_PAGES) {
                            $this->product = $node_loaded->field_product[LANGUAGE_NONE][0]['value'];
                        }
                    }
                }
                //echo "<pre>"; print_r(json_encode($info));
                //variable_set('abcdchire',variable_get('abcdchire')."--------".json_encode($info));

                $this->page_info = $info;
            }
        }
    }

    private function prepare_package_info($collection_list) {
        $references = '';
        foreach ($collection_list as $key => $value) {
            $references .= implode(',', $value) . '#';
        }

        $references = substr($references, 0, -1);

        return $references;
    }

    /*
     * Check if the event is valid.
     * Only non-empty events could be saved.
     * return boolean(true) if empty.
     */

    public function is_valid() {
        return !empty($this->page_info);
    }

    /*
     * Function to save the event in the Audit Log table.
     */

    public function save_event() {
        $page_info_json = gzdeflate(json_encode($this->page_info), 9);
        $count = FALSE;

        // Getting node title
        if (!empty($this->node->title)) {
            $node_title = $this->node->title;
        }

        if ($this->node->vid != NULL) {

            if (_learninglab_is_product_home($this->node->nid)) {
                $count = TRUE;
            }
        }
        else {
            $node_path = $this->node->field_product[LANGUAGE_NONE][0]['value'];
        }

        /* FETCH version */
        $canonical_url_val = $this->node->field_canonical_url[LANGUAGE_NONE][0]['value'];
        $vv_info = get_vv_info($canonical_url_val);

        // Check if event is a topic access or product home access
        if (($node_path == 'home' || $count) || drupal_is_front_page()) {

            $nid = db_insert('drupal_audit_log')
                ->fields(array(
                  'searchdate' => $this->date,
                  'query_term' => $this->query_term,
                  'user_track_id' => $this->audit_obj->get_user_track_id(),
                  'page_json' => $page_info_json,
                  'product' => $this->product,
                  'log_again' => 0,
                  'email_sent' => 1,
                  'search_resuts_file_created' => 1,
                  'device' => $this->device,
                  'external_content_url' => $this->page_info['accessed_url'],
                  'node_title_viewed' => $node_title,
                  'version_no' => $vv_info['version'],
                  'sp_id' => $vv_info['sp_id'],
                  'source' => $vv_info['source'],
                ))
                ->execute();
        }
        else {
            $node_title = (empty($node_title)) ? '' : $node_title;

            $nid = db_insert('drupal_audit_log')
                ->fields(array(
                  'searchdate' => $this->date,
                  'query_term' => $this->query_term,
                  'user_track_id' => $this->audit_obj->get_user_track_id(),
                  'page_json' => $page_info_json,
                  'product' => $this->product,
                  'log_again' => 0,
                  'email_sent' => 0,
                  'search_resuts_file_created' => 0,
                  'device' => $this->device,
                  'external_content_url' => $this->page_info['accessed_url'],
                  'node_title_viewed' => $node_title,
                  'version_no' => $vv_info['version'],
                  'sp_id' => $vv_info['sp_id'],
                  'source' => $vv_info['source'],
                ))
                ->execute();
        }
    }

    /*
     * Function to save the event in the file.
     */

    public function save_event_file() {
        $page_info_json = gzdeflate(json_encode($this->page_info), 9);
        $count = FALSE;
        /* FETCH version */
        $canonical_url_val = $this->node->field_canonical_url[LANGUAGE_NONE][0]['value'];
        $vv_info = get_vv_info($canonical_url_val);
        // Getting node title
        if (!empty($this->node->title)) {
            $node_title = $this->node->title;
        }

        if ($this->node->vid != NULL) {

            if (_learninglab_is_product_home($this->node->nid)) {
                $count = TRUE;
            }
        }
        else {
            $node_path = $this->node->field_product[LANGUAGE_NONE][0]['value'];
        }

        $info = new stdClass();
        // Check if event is a topic access or product home access
        if (($node_path == 'home' || $count) || drupal_is_front_page()) {
            $info->searchdate = $this->date;
            $info->query_term = $this->query_term;
            $info->user_track_id = $this->audit_obj->get_user_track_id();
            $info->page_json = $page_info_json;
            $info->product = $this->product;
            $info->log_again = 0;
            $info->email_sent = 1;
            $info->search_resuts_file_created = 1;
            $info->device = $this->device;
            $info->external_content_url = $this->page_info['accessed_url'];
            $info->node_title_viewed = $node_title;
            //Added for VV
            $info->version_no = $vv_info['version'];
            $info->sp_id = $vv_info['sp_id'];
            $info->source = $vv_info['source'];
        }
        else {
            $node_title = (empty($node_title)) ? '' : $node_title;
            $info->searchdate = $this->date;
            $info->query_term = $this->query_term;
            $info->user_track_id = $this->audit_obj->get_user_track_id();
            $info->page_json = $page_info_json;
            $info->product = $this->product;
            $info->log_again = 0;
            $info->email_sent = 0;
            $info->search_resuts_file_created = 0;
            $info->device = $this->device;
            $info->external_content_url = $this->page_info['accessed_url'];
            $info->node_title_viewed = $node_title;
            //Added for VV
            $info->version_no = $vv_info['version'];
            $info->sp_id = $vv_info['sp_id'];
            $info->source = $vv_info['source'];
        }
        _learninglab_eureka_transaction($info, 'page_json');
    }

}
