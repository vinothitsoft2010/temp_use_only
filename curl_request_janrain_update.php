<?php

$url = "https://services.janraincapture.com/entity.find";
$post_data = "client_id=**************&client_secret=*******&type_name=user&sort_on=[\"-created\"]&filter=registeredOnSite='www.janssenpro.ca'and created > '2018-07-03'"
    . "&max_results=500&attributes=[\"uuid\"]";

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

$uuid = json_decode($output, true);
echo "<pre>";

// Open CSV.
$output = '';
foreach ($uuid['results'] as $key => $val) {

    $url = "https://services.janraincapture.com/entity.update";
    $post_data = 'client_id=***********&client_secret=*********&type_name=user&uuid=' . urldecode($val['uuid'])
        . '&attributes={"emailVerified":"2018-06-13 15:43:47"}';

//trigger the update method
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
    curl_setopt($ch, CURLOPT_HEADER, FALSE);
    curl_setopt($ch, CURLOPT_POSTFIELDS, $post_data);
    $output = curl_exec($ch);
    curl_close($ch);
    if (curl_error($ch)) {
        echo 'error:' . curl_error($ch);
    }

    $json = json_decode($output, true);

    print_r($json);
    usleep(50000);
}
exit;
?>