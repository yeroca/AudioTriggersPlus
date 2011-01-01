#! /usr/bin/perl

open TRIGGER_TEXT, "<", $ARGV[0] || die "Could not open $ARGV[0]";

@triggers = <TRIGGER_TEXT>;

print "<audiotriggers>\n";

my %sound_hash = ();


foreach $trigger (@triggers) {
	chomp $trigger;

	$sound_file = $trigger;
	$sound_file =~ s/.*SoundLink=([_\.,0-9a-zA-Z ]*);.*$/\1/;
	$sound_name = $sound_file;
	$sound_name =~ s/(.*).wav$/\1/;
        $sound_hash{$sound_name} = 1;

}

my @unique_sounds = keys(%sound_hash);

foreach $sound_name (@unique_sounds) {

	print "    <sound name=\"$sound_name\">\n";
	print "        <file>C:\\Tools\\Games\\EQSounds\\$sound_name.wav</file>\n";
	print "    </sound>\n";
}


$trigger_count = 0;

foreach $trigger (@triggers) {
	chomp $trigger;


	$pattern = $trigger;
	$pattern =~ s/Trigger=(["\'\\\(\)\.,0-9a-zA-Z ]*);.*$/\1/;
	$sound_file = $trigger;
	$sound_file =~ s/.*SoundLink=([_\.,0-9a-zA-Z ]*);.*$/\1/;
	$sound_name = $sound_file;
	$sound_name =~ s/(.*).wav$/\1/;

	print "    <trigger name=\"$trigger_count\">\n";
	print "        <pattern>$pattern</pattern>\n";
	print "        <sound_to_play>$sound_name</sound_to_play>\n";
	print "    </trigger>\n";

	$trigger_count++;

}

$attach_count = 0;
print "    <logfile>\n";
print "        <file>fixme.txt</file>\n";

while ($attach_count < $trigger_count)
{
	print "        <attach_trigger name=\"$attach_count\"/>\n";
	$attach_count++;
}
print "    </logfile>\n";
print "</audiotriggers>\n";
	

