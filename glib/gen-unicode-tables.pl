#! /usr/bin/perl -w

#    Copyright (C) 1998, 1999 Tom Tromey

#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2, or (at your option)
#    any later version.

#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
#    02111-1307, USA.

# gen-unicode-tables.pl - Generate tables for libunicode from Unicode data.
# See http://www.unicode.org/Public/UNIDATA/UnicodeCharacterDatabase.html
# Usage: gen-unicode-tables.pl [-decomp | -both] UNICODE-VERSION UnicodeData.txt LineBreak.txt
# I consider the output of this program to be unrestricted.  Use it as
# you will.

# FIXME:
# * We could save even more space in the generated table by using
#   indexes and not pointers.
# * For decomp table it might make sense to use a shift count other
#   than 8.  We could easily compute the perfect shift count.

use vars qw($CODE $NAME $CATEGORY $COMBINING_CLASSES $BIDI_CATEGORY $DECOMPOSITION $DECIMAL_VALUE $DIGIT_VALUE $NUMERIC_VALUE $MIRRORED $OLD_NAME $COMMENT $UPPER $LOWER $TITLE $BREAK_CODE $BREAK_CATEGORY $BREAK_NAME);

# Names of fields in Unicode data table.
$CODE = 0;
$NAME = 1;
$CATEGORY = 2;
$COMBINING_CLASSES = 3;
$BIDI_CATEGORY = 4;
$DECOMPOSITION = 5;
$DECIMAL_VALUE = 6;
$DIGIT_VALUE = 7;
$NUMERIC_VALUE = 8;
$MIRRORED = 9;
$OLD_NAME = 10;
$COMMENT = 11;
$UPPER = 12;
$LOWER = 13;
$TITLE = 14;

# Names of fields in the line break table
$BREAK_CODE = 0;
$BREAK_PROPERTY = 1;
$BREAK_NAME = 2;

# Map general category code onto symbolic name.
%mappings =
    (
     # Normative.
     'Lu' => "G_UNICODE_UPPERCASE_LETTER",
     'Ll' => "G_UNICODE_LOWERCASE_LETTER",
     'Lt' => "G_UNICODE_TITLECASE_LETTER",
     'Mn' => "G_UNICODE_NON_SPACING_MARK",
     'Mc' => "G_UNICODE_COMBINING_MARK",
     'Me' => "G_UNICODE_ENCLOSING_MARK",
     'Nd' => "G_UNICODE_DECIMAL_NUMBER",
     'Nl' => "G_UNICODE_LETTER_NUMBER",
     'No' => "G_UNICODE_OTHER_NUMBER",
     'Zs' => "G_UNICODE_SPACE_SEPARATOR",
     'Zl' => "G_UNICODE_LINE_SEPARATOR",
     'Zp' => "G_UNICODE_PARAGRAPH_SEPARATOR",
     'Cc' => "G_UNICODE_CONTROL",
     'Cf' => "G_UNICODE_FORMAT",
     'Cs' => "G_UNICODE_SURROGATE",
     'Co' => "G_UNICODE_PRIVATE_USE",
     'Cn' => "G_UNICODE_UNASSIGNED",

     # Informative.
     'Lm' => "G_UNICODE_MODIFIER_LETTER",
     'Lo' => "G_UNICODE_OTHER_LETTER",
     'Pc' => "G_UNICODE_CONNECT_PUNCTUATION",
     'Pd' => "G_UNICODE_DASH_PUNCTUATION",
     'Ps' => "G_UNICODE_OPEN_PUNCTUATION",
     'Pe' => "G_UNICODE_CLOSE_PUNCTUATION",
     'Pi' => "G_UNICODE_INITIAL_PUNCTUATION",
     'Pf' => "G_UNICODE_FINAL_PUNCTUATION",
     'Po' => "G_UNICODE_OTHER_PUNCTUATION",
     'Sm' => "G_UNICODE_MATH_SYMBOL",
     'Sc' => "G_UNICODE_CURRENCY_SYMBOL",
     'Sk' => "G_UNICODE_MODIFIER_SYMBOL",
     'So' => "G_UNICODE_OTHER_SYMBOL"
     );

%break_mappings =
    (
     'BK' => "G_UNICODE_BREAK_MANDATORY",
     'CR' => "G_UNICODE_BREAK_CARRIAGE_RETURN",
     'LF' => "G_UNICODE_BREAK_LINE_FEED",
     'CM' => "G_UNICODE_BREAK_COMBINING_MARK",
     'SG' => "G_UNICODE_BREAK_SURROGATE",
     'ZW' => "G_UNICODE_BREAK_ZERO_WIDTH_SPACE",
     'IN' => "G_UNICODE_BREAK_INSEPARABLE",
     'GL' => "G_UNICODE_BREAK_NON_BREAKING_GLUE",
     'CB' => "G_UNICODE_BREAK_CONTINGENT",
     'SP' => "G_UNICODE_BREAK_SPACE",
     'BA' => "G_UNICODE_BREAK_AFTER",
     'BB' => "G_UNICODE_BREAK_BEFORE",
     'B2' => "G_UNICODE_BREAK_BEFORE_AND_AFTER",
     'HY' => "G_UNICODE_BREAK_HYPHEN",
     'NS' => "G_UNICODE_BREAK_NON_STARTER",
     'OP' => "G_UNICODE_BREAK_OPEN_PUNCTUATION",
     'CL' => "G_UNICODE_BREAK_CLOSE_PUNCTUATION",
     'QU' => "G_UNICODE_BREAK_QUOTATION",
     'EX' => "G_UNICODE_BREAK_EXCLAMATION",
     'ID' => "G_UNICODE_BREAK_IDEOGRAPHIC",
     'NU' => "G_UNICODE_BREAK_NUMERIC",
     'IS' => "G_UNICODE_BREAK_INFIX_SEPARATOR",
     'SY' => "G_UNICODE_BREAK_SYMBOL",
     'AL' => "G_UNICODE_BREAK_ALPHABETIC",
     'PR' => "G_UNICODE_BREAK_PREFIX",
     'PO' => "G_UNICODE_BREAK_POSTFIX",
     'SA' => "G_UNICODE_BREAK_COMPLEX_CONTEXT",
     'AI' => "G_UNICODE_BREAK_AMBIGUOUS",
     'XX' => "G_UNICODE_BREAK_UNKNOWN"
     );

# Title case mappings.
%title_to_lower = ();
%title_to_upper = ();

$do_decomp = 0;
$do_props = 1;
if ($ARGV[0] eq '-decomp')
{
    $do_decomp = 1;
    $do_props = 0;
    shift @ARGV;
}
elsif ($ARGV[0] eq '-both')
{
    $do_decomp = 1;
    shift @ARGV;
}

print "Creating decomp table\n" if ($do_decomp);
print "Creating property table\n" if ($do_props);

print "Unicode data from $ARGV[1]\n";

open (INPUT, "< $ARGV[1]") || exit 1;

$last_code = -1;
while (<INPUT>)
{
    chop;
    @fields = split (';', $_, 30);
    if ($#fields != 14)
    {
	printf STDERR ("Entry for $fields[$CODE] has wrong number of fields (%d)\n", $#fields);
    }

    $code = hex ($fields[$CODE]);

    last if ($code > 0xFFFF); # ignore characters out of the basic plane

    if ($code > $last_code + 1)
    {
	# Found a gap.
	if ($fields[$NAME] =~ /Last>/)
	{
	    # Fill the gap with the last character read,
            # since this was a range specified in the char database
	    @gfields = @fields;
	}
	else
	{
	    # The gap represents undefined characters.  Only the type
	    # matters.
	    @gfields = ('', '', 'Cn', '0', '', '', '', '', '', '', '',
			'', '', '', '');
	}
	for (++$last_code; $last_code < $code; ++$last_code)
	{
	    $gfields{$CODE} = sprintf ("%04x", $last_code);
	    &process_one ($last_code, @gfields);
	}
    }
    &process_one ($code, @fields);
    $last_code = $code;
}

@gfields = ('', '', 'Cn', '0', '', '', '', '', '', '', '',
	    '', '', '', '');
for (++$last_code; $last_code < 0x10000; ++$last_code)
{
    $gfields{$CODE} = sprintf ("%04x", $last_code);
    &process_one ($last_code, @gfields);
}
--$last_code;			# Want last to be 0xFFFF.

print "Creating line break table\n";

print "Line break data from $ARGV[2]\n";

open (INPUT, "< $ARGV[2]") || exit 1;

$last_code = -1;
while (<INPUT>)
{
    chop;

    next if /^#/;

    @fields = split (';', $_, 30);
    if ($#fields != 2)
    {
	printf STDERR ("Entry for $fields[$CODE] has wrong number of fields (%d)\n", $#fields);
    }

    $code = hex ($fields[$CODE]);

    last if ($code > 0xFFFF); # ignore characters out of the basic plane

    if ($code > $last_code + 1)
    {
	# Found a gap.
	if ($fields[$NAME] =~ /Last>/)
	{
	    # Fill the gap with the last character read,
            # since this was a range specified in the char database
          $gap_break_prop = $fields[$BREAK_PROPERTY];
          for (++$last_code; $last_code < $code; ++$last_code)
            {
              $break_props[$last_code] = $gap_break_prop;
            }
	}
	else
	{
          # The gap represents undefined characters. If assigned,
          # they are AL, if not assigned, XX
          for (++$last_code; $last_code < $code; ++$last_code)
            {
              if ($type[$last_code] eq 'Cn')
                {
                  $break_props[$last_code] = 'XX';
                }
              else
                {
                  $break_props[$last_code] = 'AL';
                }
            }
	}
    }
    $break_props[$code] = $fields[$BREAK_PROPERTY];
    $last_code = $code;
}

for (++$last_code; $last_code < 0x10000; ++$last_code)
{
  if ($type[$last_code] eq 'Cn')
    {
      $break_props[$last_code] = 'XX';
    }
  else
    {
      $break_props[$last_code] = 'AL';
    }
}
--$last_code;			# Want last to be 0xFFFF.

print STDERR "Last code is not 0xFFFF" if ($last_code != 0xFFFF);

&print_tables ($last_code)
    if $do_props;
&print_decomp ($last_code)
    if $do_decomp;

&print_line_break ($last_code);

exit 0;

# Process a single character.
sub process_one
{
    my ($code, @fields) = @_;

    $type[$code] = $fields[$CATEGORY];
    if ($type[$code] eq 'Nd')
    {
	$value[$code] = int ($fields[$DECIMAL_VALUE]);
    }
    elsif ($type[$code] eq 'Ll')
    {
	$value[$code] = hex ($fields[$UPPER]);
    }
    elsif ($type[$code] eq 'Lu')
    {
	$value[$code] = hex ($fields[$LOWER]);
    }

    if ($type[$code] eq 'Lt')
    {
	$title_to_lower{$code} = hex ($fields[$LOWER]);
	$title_to_upper{$code} = hex ($fields[$UPPER]);
    }

    $cclass[$code] = $fields[$COMBINING_CLASSES];

    # Handle decompositions.
    if ($fields[$DECOMPOSITION] ne ''
	&& $fields[$DECOMPOSITION] !~ /\<.*\>/)
    {
	$decompositions[$code] = $fields[$DECOMPOSITION];
    }
}

sub print_tables
{
    my ($last) = @_;
    my ($outfile) = "gunichartables.h";

    local ($bytes_out) = 0;

    print "Writing $outfile...\n";

    open (OUT, "> $outfile");

    print OUT "/* This file is automatically generated.  DO NOT EDIT!\n";
    print OUT "   Instead, edit gen-unicode-tables.pl and re-run.  */\n\n";

    print OUT "#ifndef CHARTABLES_H\n";
    print OUT "#define CHARTABLES_H\n\n";

    print OUT "#define G_UNICODE_DATA_VERSION \"$ARGV[0]\"\n\n";

    printf OUT "#define G_UNICODE_LAST_CHAR 0x%04x\n\n", $last;

    for ($count = 0; $count <= $last; $count += 256)
    {
	$row[$count / 256] = &print_row ($count, '(char *) ', 'char', 1,
					 'page', \&fetch_type);
    }

    print OUT "static char *type_table[256] = {\n";
    for ($count = 0; $count <= $last; $count += 256)
    {
	print OUT ",\n" if $count > 0;
	print OUT "  ", $row[$count / 256];
	$bytes_out += 4;
    }
    print OUT "\n};\n\n";


    #
    # Now print attribute table.
    #

    for ($count = 0; $count <= $last; $count += 256)
    {
	$row[$count / 256] = &print_row ($count, '', 'unsigned short', 2,
					 'attrpage', \&fetch_attr);
    }
    print OUT "static unsigned short *attr_table[256] = {\n";
    for ($count = 0; $count <= $last; $count += 256)
    {
	print OUT ",\n" if $count > 0;
	print OUT "  ", $row[$count / 256];
	$bytes_out += 4;
    }
    print OUT "\n};\n\n";

    # FIXME: type.
    print OUT "static unsigned short title_table[][3] = {\n";
    my ($item);
    my ($first) = 1;
    foreach $item (sort keys %title_to_lower)
    {
	print OUT ",\n"
	    unless $first;
	$first = 0;
	printf OUT "  { 0x%04x, 0x%04x, 0x%04x }", $item, $title_to_upper{$item}, $title_to_lower{$item};
	$bytes_out += 6;
    }
    print OUT "\n};\n\n";

    print OUT "#endif /* CHARTABLES_H */\n";

    close (OUT);

    printf STDERR "Generated %d bytes in tables\n", $bytes_out;
}

# A fetch function for the type table.
sub fetch_type
{
    my ($index) = @_;
    return $mappings{$type[$index]};
}

# A fetch function for the attribute table.
sub fetch_attr
{
    my ($index) = @_;
    if (defined $value[$index])
      {
        return sprintf ("0x%04x", $value[$index]);
      }
    else
      {
        return "0x0000";
      }
}

# Print a single "row" of a two-level table.
sub print_row
{
    my ($start, $def_pfx, $typname, $typsize, $name, $fetcher) = @_;

    my ($i);
    my (@values);
    my ($flag) = 1;
    my ($off);

    for ($off = 0; $off < 256; ++$off)
    {
	$values[$off] = $fetcher->($off + $start);
	if ($values[$off] ne $values[0])
	{
	    $flag = 0;
	}
    }
    if ($flag)
    {
	return $def_pfx . $values[0];
    }

    printf OUT "static %s %s%d[256] = {\n  ", $typname, $name, $start / 256;
    my ($column) = 2;
    for ($i = $start; $i < $start + 256; ++$i)
    {
	print OUT ", "
	    if $i > $start;
	my ($text) = $values[$i - $start];
	if (length ($text) + $column + 2 > 78)
	{
	    print OUT "\n  ";
	    $column = 2;
	}
	print OUT $text;
	$column += length ($text) + 2;
    }
    print OUT "\n};\n\n";

    $bytes_out += 256 * $typsize;

    return sprintf "%s%d", $name, $start / 256;
}

# Generate the character decomposition header.
sub print_decomp
{
    my ($last) = @_;
    my ($outfile) = "gunidecomp.h";

    local ($bytes_out) = 0;

    print "Writing $outfile...\n";

    open (OUT, "> $outfile") || exit 1;

    print OUT "/* This file is automatically generated.  DO NOT EDIT! */\n\n";
    print OUT "#ifndef DECOMP_H\n";
    print OUT "#define DECOMP_H\n\n";

    printf OUT "#define G_UNICODE_LAST_CHAR 0x%04x\n\n", $last;

    my ($count, @row);
    for ($count = 0; $count <= $last; $count += 256)
    {
	$row[$count / 256] = &print_row ($count, '(unsigned char *) ',
					 'unsigned char', 1, 'cclass',
					 \&fetch_cclass);
    }

    print OUT "static unsigned char *combining_class_table[256] = {\n";
    for ($count = 0; $count <= $last; $count += 256)
    {
	print OUT ",\n" if $count > 0;
	print OUT "  ", $row[$count / 256];
	$bytes_out += 4;
    }
    print OUT "\n};\n\n";

    print OUT "typedef struct\n{\n";
    # FIXME: type.
    print OUT "  unsigned short ch;\n";
    print OUT "  unsigned char *expansion;\n";
    print OUT "} decomposition;\n\n";

    print OUT "static decomposition decomp_table[] =\n{\n";
    my ($iter);
    my ($first) = 1;
    for ($count = 0; $count <= $last; ++$count)
    {
	if (defined $decompositions[$count])
	{
	    print OUT ",\n"
		if ! $first;
	    $first = 0;
	    printf OUT "  { 0x%04x, \"", $count;
	    $bytes_out += 2;
	    foreach $iter (&expand_decomp ($count))
	    {
		printf OUT "\\x%02x\\x%02x", $iter / 256, $iter & 0xff;
		$bytes_out += 2;
	    }
	    # Only a single terminator because one is implied in the string.
	    print OUT "\\0\" }";
	    $bytes_out += 2;
	}
    }
    print OUT "\n};\n\n";

    print OUT "#endif /* DECOMP_H */\n";

    printf STDERR "Generated %d bytes in decomp tables\n", $bytes_out;
}

sub print_line_break
{
    my ($last) = @_;
    my ($outfile) = "gunibreak.h";

    local ($bytes_out) = 0;

    print "Writing $outfile...\n";

    open (OUT, "> $outfile");

    print OUT "/* This file is automatically generated.  DO NOT EDIT!\n";
    print OUT "   Instead, edit gen-unicode-tables.pl and re-run.  */\n\n";

    print OUT "#ifndef BREAKTABLES_H\n";
    print OUT "#define BREAKTABLES_H\n\n";

    print OUT "#define G_UNICODE_DATA_VERSION \"$ARGV[0]\"\n\n";

    printf OUT "#define G_UNICODE_LAST_CHAR 0x%04x\n\n", $last;

    for ($count = 0; $count <= $last; $count += 256)
    {
	$row[$count / 256] = &print_row ($count, '(char *) ', 'char', 1,
					 'page',
                                         \&fetch_break_type);
    }

    print OUT "static char *break_property_table[256] = {\n";
    for ($count = 0; $count <= $last; $count += 256)
    {
	print OUT ",\n" if $count > 0;
	print OUT "  ", $row[$count / 256];
	$bytes_out += 4;
    }
    print OUT "\n};\n\n";

    print OUT "#endif /* BREAKTABLES_H */\n";

    close (OUT);

    printf STDERR "Generated %d bytes in break tables\n", $bytes_out;
}


# A fetch function for the break properties table.
sub fetch_break_type
{
    my ($index) = @_;
    return $break_mappings{$break_props[$index]};
}

# Fetcher for combining class.
sub fetch_cclass
{
    my ($i) = @_;
    return $cclass[$i];
}

# Expand a character decomposition recursively.
sub expand_decomp
{
    my ($code) = @_;

    my ($iter, $val);
    my (@result) = ();
    foreach $iter (split (' ', $decompositions[$code]))
    {
	$val = hex ($iter);
	if (defined $decompositions[$val])
	{
	    push (@result, &expand_decomp ($val));
	}
	else
	{
	    push (@result, $val);
	}
    }

    return @result;
}
