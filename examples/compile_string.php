<?php
  /* Compile a simple Hello World type program */

  $oparray = parsekit_compile_string('

function foo($bar = "Hello World") {
  return $bar;
}

echo foo();

');

  var_dump($oparray);
