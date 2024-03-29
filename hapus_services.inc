<?php
/**
 * Determine whether the current user can access a node resource.
 *
 * @param $op
 *   One of view, update, create, delete per user_access().
 * @param $args
 *   Resource arguments passed through from the original request.
 * @return bool
 *
 * @see user_access()
 */
function _hapus_services_resource_access($op = 'create', $args=array()) {
	return user_access($op);
}


function _hapus_services_resource_create($type, $resource,$key,$username,
  $parameters = " --dpi 200 --page-size A4 ",$outputasfile = 0){
	$return = array($type,$resource);
  global $user;

  if (!$user->uid) {
    $uid = user_authenticate($username, $key);

    if ($uid) {
      $user = user_load($uid);
      if ($user->uid) {
        user_login_finalize();
      }
    }else{
        watchdog('user', 'Invalid login attempt for %username.', array('%username' => $username));
        return services_error(t('Wrong username or key.'), 401);
    }
  }
  $profile= profile2_load_by_user($user,'main');

  $query = new EntityFieldQuery();
  $count = $query->entityCondition('entity_type', 'file')->count()
  ->propertyCondition('uid',$user->uid)->propertyCondition('timestamp',array( mktime(0,0,0,date('n'),01,date('Y')),mktime(0,0,0,((date('n')) + 1),0,date('Y'))) , 'BETWEEN' )->execute();
  if($profile->field_user_monthly_limit[$user->language?$user->language:LANGUAGE_NONE][0]['value'] < ($count+1) ){
  	return services_error(t('You have exceeded your mothly limit.'), 403);
  }

  if(strtolower($type) == "html"){
    $sourcefile->file = $resource;
      // If the file data is empty then bail
    if (!isset($sourcefile->file)) {
      return services_error(t('Resource is not valid.'), 400);
    }
  } else if(strtolower($type) == "url") {
    $resourceData = drupal_http_request($resource);
    if($resourceData->code !=200){
      return services_error(t('Resource URL is not valid.'), 400);
    }else {
      $sourcefile->file = $resourceData->data;
    }
    // if(valid_url($resource,TRUE) != TRUE){
    //   return services_error(t('Resource URL is not valid.'), 400);
    // }
  } else {
    return services_error(t('Type is not valid. It can either be HTML or URL.'), 400);
  }

  // Make sure we create new file.
  $sourcefile->fid = NULL;

  //Set the file name which will be the key to retrive file info in
  //the download process.
  $hash = drupal_hash_base64($sourcefile->file.$parameters);
  $sourcefile->filename =  $hash.".html" ;
  $sourcefile->uid = $user->uid;

  $dir = file_default_scheme() . '://';


  // Get the directory name for the location of the file:
  $sourcefile->filepath = file_default_scheme() . '://' . $sourcefile->filename;

  $sourceRealPath =drupal_realpath($sourcefile->filepath);
  if (is_file($sourceRealPath)|| is_file(str_replace(".html",".pdf",$sourceRealPath))) {
      // Do something
    module_load_include("inc","file","file");
    $destination = file_load_multiple(array(),
                  array("filename" => $hash.".pdf"));
    $destinationFile = array_shift($destination);
    return _hapus_services_return_file($destinationFile,$outputasfile);
    return array(
      'fid' => $destination[0],
      'uri' => services_resource_uri(array('file', $destination[0])),
    );
  }else{

      // Build the destination folder tree if it doesn't already exists.
    if (!file_prepare_directory($dir, FILE_CREATE_DIRECTORY)) {
      return services_error(t("Could not create destination directory for file."), 500);
    }

    // Update the timestamp to the current time, otherwise the file could
    // get deleted on the next cron run if its status is set to 0.
    $sourcefile->timestamp = time();
    if(strtolower($type) == "html"){
      $sourcefile->filemime = 'text/html';
      // Write the file
      if (!$sourcefile = file_save_data($sourcefile->file, $sourcefile->filepath)) {
        return services_error(t("Could not write file to destination"), 500);
      }

      // Required to be able to reference this file.
      file_usage_add($sourcefile, 'services', 'files', $sourcefile->fid);
    }
    //Starting generation of pdf
    $destinationFile = new stdClass();
    $destinationFile->filename =  $hash.".pdf";
    $destinationFile->uid = $user->uid;
    $destinationFile->filepath = file_default_scheme() . '://' . $destinationFile->filename;
    $destinationFile->timestamp = time();
    $destinationRealPath = drupal_realpath($destinationFile->filepath);
    $destinationFile->fid = NULL;
    if(strtolower($type) == "url"){
      $sourceRealPath = $resource;
    }
    $cmd =  'xvfb-run -a -s "-screen 0 640x480x16" wkhtmltopdf  '.$parameters.
            ' '.$sourceRealPath.' '.$destinationRealPath;

    exec($cmd);
    $destinationFileContents = file_get_contents($destinationRealPath);
    $removeFileCmd = "rm -fr $destinationRealPath";
    exec($removeFileCmd);
    $destinationFile->filemime = file_get_mimetype($destinationFile->filename);
    $destinationFile->file = $destinationFileContents;
    // Write the file
    if (!$destinationFile = file_save_data($destinationFile->file, $destinationFile->filepath)) {
      return services_error(t("Could not write file to destination"), 500);
    }

    // Required to be able to reference this file.
    file_usage_add($destinationFile, 'services', 'files', $destinationFile->fid);

    $destinationFile->file = $destinationFileContents;



    // send the response
    return _hapus_services_return_file($destinationFile,$outputasfile);
  }
	//return $return;
}

function _hapus_services_return_file($file,$outputasfile){
  $filepath = $file->uri;

  // Convert the uri to the external url path provided by the stream wrapper.
  $file->uri_full = file_create_url($file->uri);

  //return
  if(isset($file->file)){
    if($outputasfile){
      echo $file->file;
      exit();
    }
    $file->file =  base64_encode($file->file);
  }else{
    $fileData = file_get_contents(drupal_realpath($filepath));
    if($outputasfile){
      echo $fileData;
      exit();
    }
    $file->file = base64_encode($fileData);
  }

  // Provide a path in the form sample/test.txt.
  $file->target_uri = file_uri_target($file->uri);

  return $file;
}
function _hapus_services_resource_retrieve($username,$key){

	global $user;
  if (!$user->uid) {
    $uid = user_authenticate($username, $key);

    if ($uid) {
      $user = user_load($uid);
      if ($user->uid) {
        user_login_finalize();
      }
    }else{
        watchdog('user', 'Invalid login attempt for %username.', array('%username' => $username));
        return services_error(t('Wrong username or key.'), 401);
    }
  }
	module_load_include("inc","services","resources/user_resource");
	$account = _user_resource_retrieve($user->uid);
  $profile= profile2_load_by_user($account,'main');

  unset($profile->uid,$profile->pid,$account->roles,$account->rdf_mapping);
  $account->account_details= $profile;
  return $account;

}



?>
