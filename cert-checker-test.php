<?php

if (php_sapi_name() !== "cli") {
    echo "Please run in cli.";
    die();
}
//The list of sites. Use the csv instead of hardcode
$watchCerts = array
  (
  "prod-example1.com" => array("ucc" => true, "expire" => true),
  "prod-example2.com" => array("ucc" => true, "expire" => true),
  "prod-example3.com" => array("ucc" => true, "expire" => true),
  "prod-example4.com" => array("ucc" => true, "expire" => true)
);

$runCount = 0;
echo "Date Run,Domain,Issuer,Cert Name,Cert Subject,Expiration Date\n";
foreach ($watchCerts as $domain => $args) {
    $runDate = date('Y/m/d H:i:s', time());
    try {
        $g = stream_context_create(array("ssl" => array("capture_peer_cert" => true)));
        $r = stream_socket_client("ssl://{$domain}:443", $errno, $errstr, 30, STREAM_CLIENT_CONNECT, $g);
        $cont = stream_context_get_params($r);
        $certBundle = openssl_x509_parse($cont["options"]["ssl"]["peer_certificate"]);
    }
    catch (Exception $e) {
        echo "{$runDate},{$domain},(failed ssl error)\n";
        continue;
    }
    $expireDate = date('Y/m/d H:i:s', $certBundle["validTo_time_t"]);
    echo "{$runDate},{$domain},{$certBundle["issuer"]["C"]}-{$certBundle["issuer"]["O"]}-{$certBundle["issuer"]["OU"]}-{$certBundle["issuer"]["CN"]},{$certBundle["name"]},{$certBundle["subject"]["CN"]},{$expireDate}\n";

    usleep(100);

    continue;
}
