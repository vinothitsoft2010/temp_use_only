<?php

$output = '';
//NA service endpoint
$url = "https://services.janraincapture.com/entity.find";

//NA request data
$post_data = "client_id=*********&client_secret=************&type_name=user&sort_on=[\"-created\"]&filter=registeredOnSite='www.cancer.com' or logins.domain='www.cancer.com'"
    . "&max_results=2000&attributes=[\"uuid\",\"familyName\",\"givenName\",\"email\", \"emailVerified\",\"created\",\"lastLogin\",\"lastUpdated\",\"primaryAddress.address1\",\"primaryAddress.address2\",\"primaryAddress.city\",\"primaryAddress.stateAbbreviation\",\"primaryAddress.zip\",\"registeredOnSite\",\"logins\"]";

//LATAM request data
$post_data = "client_id=*******&client_secret=*****&type_name=user&sort_on=[\"-created\"]&filter=registeredOnSite='www.cancer.com' or logins.domain='www.cancer.com'"
    . "&max_results=5000&attributes=[\"uuid\",\"CODSID\",\"familyName\",\"givenName\",\"email\", \"emailVerified\",\"created\",\"lastLogin\",\"lastUpdated\",\"primaryAddress.address1\",\"primaryAddress.address2\",\"primaryAddress.city\",\"primaryAddress.country\",\"primaryAddress.stateAbbreviation\",\"primaryAddress.zip\","
    . "\"professionalData.accountStatus\",\"professionalData.documentation\",\"professionalData.documentationType\",\"professionalData.practiceState\",\"professionalData.professionType\",\"professionalData.specialty\",\"registeredOnSite\",\"logins\"]";


//Curl request to fetch the user details
$ch = curl_init($url);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
curl_setopt($ch, CURLOPT_HEADER, FALSE);
curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
curl_setopt($ch, CURLOPT_POSTFIELDS, $post_data);
$output = curl_exec($ch);
if (curl_error($ch)) {
    echo 'error:' . curl_error($ch);
}
curl_close($ch);

//Decodes the results
$json = json_decode($output, true);

$header = array("uuid", "CODSID", "familyName", "givenName", "email", "emailVerified", "address1", "address2", "city", "country", "stateAbbreviation", "zip", "accountStatus", "documentation", "documentationType", "practiceState", "professionType", "specialty", "created", "lastLogin", "lastUpdated", "registeredOnSite", "login count");
echo implode("\t", array_values($header)) . "\r\n";
// filename for download
$filename = "website_data_" . date('Ymd') . ".xls";

header("Content-Disposition: attachment; filename=\"$filename\"");
header("Content-Type: application/vnd.ms-excel");

$flag = false;
foreach ($json['results'] as $rows) {
    $row['uuid'] = $rows['uuid'];
    $row['CODSID'] = $rows['CODSID'];
    $row['familyName'] = $rows['familyName'];
    $row['givenName'] = $rows['givenName'];
    $row['email'] = $rows['email'];
    $row['emailVerified'] = $rows['emailVerified'];
    $row['address1'] = $rows['primaryAddress']['address1'] . ' ';
    $row['address2'] = $rows['primaryAddress']['address2'];
    $row['city'] = $rows['primaryAddress']['city'];
    $row['country'] = $rows['primaryAddress']['country'];
    $row['stateAbbreviation'] = $rows['primaryAddress']['stateAbbreviation'];
    $row['zip'] = $rows['primaryAddress']['zip'];
    $row['accountStatus'] = $rows['professionalData']['accountStatus'] . '';
    $row['documentation'] = $rows['professionalData']['documentation'] . '';
    $row['documentationType'] = $rows['professionalData']['documentationType'] . '';
    $row['practiceState'] = $rows['professionalData']['practiceState'] . '';
    $row['professionType'] = $rows['professionalData']['professionType'] . '';
    $row['specialty'] = $rows['professionalData']['specialty'] . '';
    $row['created'] = $rows['created'];
    $row['lastLogin'] = $rows['lastLogin'];
    $row['lastUpdated'] = $rows['lastUpdated'];
    $row['registeredOnSite'] = $rows['registeredOnSite'];
    $row['login_count'] = $rows['logins'] ? $rows['logins'][0]['loginCount'] : "";

    //array_walk($row, __NAMESPACE__ . '\cleanData');
    echo implode("\t", array_values($row)) . "\r\n";
}

function cleanData(&$str) {
    // escape tab characters
    $str = preg_replace("/\t/", "\\t", $str);

    // escape new lines
    $str = preg_replace("/\r?\n/", "\\n", $str);

    // convert 't' and 'f' to boolean values
    if ($str == 't')
        $str = 'TRUE';
    if ($str == 'f')
        $str = 'FALSE';

    // force certain number/date formats to be imported as strings
    if (preg_match("/^0/", $str) || preg_match("/^\+?\d{8,}$/", $str) || preg_match("/^\d{4}.\d{1,2}.\d{1,2}/", $str)) {
        $str = "'$str";
    }

    // escape fields that include double quotes
    if (strstr($str, '"'))
        $str = '"' . str_replace('"', '""', $str) . '"';
}

exit;
?>