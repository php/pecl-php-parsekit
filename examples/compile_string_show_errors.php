<?php
  /* Compile a buggy Hello World program */

  $oparray = parsekit_compile_string('

function foo($bar = "Hello World) {
  return $bar;
}

echo foo();

', $errors);

  var_dump($errors);
