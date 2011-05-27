# glib-build.mk contains support for using the various
# code/data-building binaries installed by glib: glib-mkenums,
# glib-genmarshal, glib-compile-schemas, and glib-compile-resources.
# (It does not currently contain support for gdbus-codegen.)


# GLIB-MKENUMS
#
# glib-build.mk will automatically build/update "foo-enum-types.c" and
# "foo-enum-types.h" (or "fooenumtypes.c" and "fooenumtypes.h") as
# needed, if there is an appropriate $(foo_enum_types_sources) /
# $(fooenumtypes_sources) variable indicating the source files to read
# enums from.
#
# For your convenience, any .c files or glib-build.mk-generated
# sources in the _sources variable will be ignored. This means you can
# usually just set it to the value of your library/program's _HEADERS
# and/or _SOURCES variables, even if those variables contain
# "foo-enum-types.h", etc.
#
# You can set GLIB_MKENUMS_H_FLAGS and GLIB_MKENUMS_C_FLAGS (or an
# appropriate file-specific variable, eg
# foo_enum_types_MKENUMS_H_FLAGS) to set/override certain glib-mkenums
# options. In particular, you can do things like:
#
#     GLIB_MKENUMS_C_FLAGS = --fhead "\#define FOO_ENABLE_UNSTABLE_API"
#
# (The backslash is necessary to keep make from thinking the "#" is
# the start of a comment.)
#
# Note that enums files should not be disted (since the set of
# available enum types may depend on configure-time options, and they
# can always be generated at build time anyway), so the generated
# files should be added to a "nodist_" SOURCES variable. You do not
# need to add the generated files to BUILT_SOURCES or CLEANFILES;
# glib-build.mk will cause them to be built and cleaned at the correct
# times (but note that it does not actually modify BUILT_SOURCES or
# CLEANFILES). You may want to add "foo-enum-types.h",
# "foo-enum-types.c", "foo-enum-types.h.stamp", and
# "foo-enum-types.c.stamp" to your .gitignore.


# GLIB-GENMARSHAL
#
#   NOTE: the use of explicit marshallers is somewhat deprecated.
#   Consider using g_cclosure_marshal_generic() instead.
#
# glib-build.mk will automatically build/update "foo-marshal.c" and
# "foo-marshal.h" (or "foomarshal.c" and "foomarshal.h") as needed, if
# there is an appropriate $(foo_marshal_sources) /
# $(foomarshal_sources) variable indicating the source files to use.
# You do not need to explicitly manage the list of marshallers;
# glib-build.mk will automatically generate a "foo-marshal.list" file
# containing all "_foo_marshal_*" functions referenced from
# $(foo_marshal_sources), and will then regenerate the .c and .h files
# whenever the list changes.
#
# For your convenience, any .h files or glib-build.mk-generated files
# in the _sources variable will be ignored. This means you can usually
# just set foo_marshal_sources to the value of your library/program's
# _SOURCES variable, even if that variable contains "foo-marshal.c",
# etc.
#
# You can set GLIB_GENMARSHAL_H_FLAGS and GLIB_GENMARSHAL_C_FLAGS (or
# an appropriate file-specific variable, eg
# foo_marshal_GENMARSHAL_H_FLAGS) to set/override certain
# glib-genmarshal options.
#
# Note that marshal files should not be disted (since the set of
# required marshallers may depend on configure-time options, and they
# can always be generated at build time anyway), so the generated
# files should be added to a "nodist_" SOURCES variable. You do not
# need to add the generated files to BUILT_SOURCES or CLEANFILES;
# glib-build.mk will cause them to be built and cleaned at the correct
# times (but note that it does not actually modify BUILT_SOURCES or
# CLEANFILES). You may want to add "foo-marshal.h", "foo-marshal.c",
# "foo-marshal.list", and "foo-marshal.list.stamp" to your .gitignore.


# GLIB-COMPILE-SCHEMAS
#
# Any foo.gschemas.xml files listed in gsettingsschema_DATA will be
# validated before installation, and normally will be compiled after
# installation. (If you include the GLIB_GSETTINGS rule from
# gsettings.m4 in your configure.ac, then glib-build.mk will obey the
# "--disable-schema-compile" flag just like the older GLIB_GSETTINGS
# code did.)
#
# glib-build.mk will automatically build/update any
# "org.foo.bar.enums.xml" files in gsettingsschema_DATA, if there is
# an appropriate $(org_foo_bar_enums_xml_sources) variable indicating
# the source files to use. All enums files will automatically be built
# before any schema files are validated.


# GLIB-COMPILE-RESOURCES
#
# glib-build.mk will automatically build/update "foo-resources.h"
# and/or "foo-resources.c" (or "fooresources.h" and/or
# "fooresources.c") as needed, if there is an appropriate
# $(foo_resources_sources) / $(fooresources_sources) variable
# indicating the source .gresource.xml file to use.
#
# If you specify both a .c and a .h file, glib-build.mk will compile
# the resource with the --manual-register flag. If you specify only a
# .c file, it will not.
#
# Alternatively, you can build a standalone "foo.gresource" file by
# specifying $(foo_gresource_sources).
#
# You can set GLIB_COMPILE_RESOURCES_FLAGS (or an appropriate
# file-specific variable, eg foo_resources_COMPILE_RESOURCES_FLAGS or
# foo_gresource_COMPILE_RESOURCES_FLAGS) to set/override
# glib-compile-resources options.
#
# Although there may be situations where you want to dist a resource
# file and not dist its sources, glib-build.mk does not currently
# support this; generated resource files should be added to a
# "nodist_" SOURCES variable. glib-build.mk will ensure that the .xml
# file and any files it includes will get disted.
#
# You do not need to add resource files to BUILT_SOURCES or
# CLEANFILES; glib-build.mk will cause them to be built and cleaned at
# the correct time (but note that it does not actually modify
# BUILT_SOURCES or CLEANFILES). You may want to add "foo-resources.c" /
# "foo-resources.h" / "foo.gresource" to .gitignore.


###


# This is like AM_V_GEN, except that it strips ".stamp" from its output
_GLIB_V_GEN = $(_glib_v_gen_$(V))
_glib_v_gen_ = $(_glib_v_gen_$(AM_DEFAULT_VERBOSITY))
_glib_v_gen_0 = @echo "  GEN     " $(subst .stamp,,$@);

# _glib_all_sources contains every file that is (directly or
# indirectly) part of any _SOURCES variable in Makefile.am. We use
# this to find the files we need to generate rules for. (We can't just
# use '%' rules to build things because then the .stamp files get
# treated as "intermediate files" by make, and then things don't
# always get rebuilt when we need them to be.)
#
# Likewise, _glib_all_data contains all of the installed data files.
# (ie, all of the "foodir_DATA" variables, but not "INSTALL_DATA" or
# "install_sh_DATA", which are unrelated).
#
# ($(sort) is used here (and in many other places below) only for its
# side effect of removing duplicates.)
_glib_all_sources = $(sort $(foreach var,$(filter %_SOURCES,$(.VARIABLES)),$($(var))))
_glib_all_data = $(sort $(foreach var,$(filter-out INSTALL_DATA install_sh_DATA,$(filter %_DATA,$(.VARIABLES))),$($(var))))

# We can't add our generated files to BUILT_SOURCES because that would
# create recursion with _glib_all_sources. So we implement our own
# equivalent rules.
_glib_built_sources =
all: _glib-ensure-built-sources
check: _glib-ensure-built-sources
install: _glib-ensure-built-sources

# We add dependencies to this rule in the various generators below; we
# can't just make the rule depend on $(_glib_built_sources) because
# that value won't have been computed yet at the point when make
# figures out rule dependencies.
_glib-ensure-built-sources:

# We want to make "clean-am" depend on "_glib-clean". But if automake
# sees us doing that, it will think we're trying to *override*
# clean-am, and so it won't output the actual clean-am rule. So we
# hide from automake by prefacing the rule with ":::", and then use
# $(eval) to get make to apply it.
define _glib_clean_am_hidden
:::clean-am: _glib-clean
endef
$(eval $(subst :::,,$(_glib_clean_am_hidden)))

_glib-clean:
	@$(if $(strip $(_glib_clean_files)),rm -f $(sort $(_glib_clean_files)),:)
_glib_clean_files =

# OTOH, we DO want automake to see us using dist-hook, since otherwise
# it won't generate the rules that use it.
dist-hook: _glib-dist

# We have to use $(realpath) here so that "x" and "./x" will be
# recognized as duplicates by $(sort), since the cp will fail if given
# the same file twice.
_glib-dist: _glib-ensure-built-sources
	@$(if $(strip $(_glib_dist_files)),cp -p $(sort $(realpath $(_glib_dist_files))) $(distdir)/,:)


### GLIB-MKENUMS

# These are used as macros (with the value of $(1) inherited from the
# "caller")
#   _glib_enum_types_prefix("foo-enum-types") = "foo_enum_types"
#   _glib_enum_types_guard("foo-enum-types") = "__FOO_ENUM_TYPES_H__"
#   _glib_enum_types_sources_var("foo-enum_types") = "foo_enum_types_sources"
#   _glib_enum_types_sources = the filtered value of $(foo_enum_types_sources)
#   _glib_enum_types_h_sources = the .h files in $(_glib_enum_types_sources)
_glib_enum_types_prefix = $(subst -,_,$(notdir $(1)))
_glib_enum_types_guard = __$(shell LC_ALL=C echo $(_glib_enum_types_prefix) | tr 'a-z' 'A-Z')_H__
_glib_enum_types_sources_var = $(_glib_enum_types_prefix)_sources
_glib_enum_types_sources = $(filter-out $(_glib_built_sources) $(1).h,$($(_glib_enum_types_sources_var)))
_glib_enum_types_h_sources = $(filter %.h,$(_glib_enum_types_sources))

# _glib_all_enum_types contains the basenames (eg, "fooenumtypes",
# "bar-enum-types") of all enum-types files known to the Makefile.
# _glib_generated_enum_types contains only the ones being generated by
# glib-build.mk.
_glib_all_enum_types = $(subst .h,,$(notdir $(filter %enum-types.h %enumtypes.h,$(_glib_all_sources))))
_glib_generated_enum_types = $(foreach f,$(_glib_all_enum_types),$(if $(strip $(call _glib_enum_types_sources,$f)),$f))

# _glib_make_mkenums_rules is a multi-line macro that outputs a set of
# rules for a single .h/.c pair (whose basename is $(1)). automake
# doesn't recognize GNU make's define/endef syntax, so if we defined
# the macro directly, it would try to, eg, add the literal "$(1).h" to
# _glib_clean_files. So we hide the macro by prefixing each line with
# ":::", as with _glib_clean_am_hidden above.

# We have to include "Makefile" in the dependencies so that the
# outputs get regenerated when you remove files from
# foo_enum_types_sources. (This is especially important for
# foo-enum-types.h, which might otherwise try to #include files that
# no longer exist.).

# Several of the variables/macros in the rule use double $$, so they
# get expanded when *running* the rule rather than when *generating*
# it, because they use _glib_built_sources, which won't be up-to-date
# when the rule is initially generated

define _glib_make_mkenums_rules_hidden
:::$(1).h.stamp: $$(call _glib_enum_types_h_sources,$(1)) Makefile
:::	$$(_GLIB_V_GEN) $$(GLIB_MKENUMS) \
:::		--fhead "/* Generated by glib-mkenums. Do not edit */\n\n" \
:::		--fhead "#ifndef $(_glib_enum_types_guard)\n" \
:::		--fhead "#define $(_glib_enum_types_guard)\n\n" \
:::		$$(GLIB_MKENUMS_H_FLAGS) \
:::		$$($(_glib_enum_types_prefix)_MKENUMS_H_FLAGS) \
:::		--fhead "#include <glib-object.h>\n\n" \
:::		--fhead "G_BEGIN_DECLS\n" \
:::		--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n" \
:::		--vhead "#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ (@enum_name@_get_type ())\n\n" \
:::		--ftail "G_END_DECLS\n\n#endif /* $(_glib_enum_types_guard) */" \
:::		$$(filter-out Makefile, $$^) > $(1).h.tmp && \
:::	(cmp -s $(1).h.tmp $(1).h || cp $(1).h.tmp $(1).h) && \
:::	rm -f $(1).h.tmp && \
:::	echo timestamp > $$@
:::
:::$(1).h: $(1).h.stamp
:::	@true
:::
:::$(1).c.stamp: $$(call _glib_enum_types_h_sources,$(1)) Makefile
:::	$$(_GLIB_V_GEN) $$(GLIB_MKENUMS) \
:::		--fhead "/* Generated by glib-mkenums. Do not edit */\n\n" \
:::		--fhead "#ifdef HAVE_CONFIG_H\n" \
:::		--fhead "#include \"config.h\"\n" \
:::		--fhead "#endif\n\n" \
:::		--fhead "#include \"$(notdir $(1)).h\"\n" \
:::		$$(GLIB_MKENUMS_C_FLAGS) \
:::		$$($(_glib_enum_types_prefix)_MKENUMS_C_FLAGS) \
:::		--fhead "$$(foreach f,$$(filter-out Makefile,$$(^F)),\n#include \"$$(f)\")\n\n" \
:::		--vhead "GType\n" \
:::		--vhead "@enum_name@_get_type (void)\n" \
:::		--vhead "{\n" \
:::		--vhead "  static volatile gsize g_define_type_id__volatile = 0;\n\n" \
:::		--vhead "  if (g_once_init_enter (&g_define_type_id__volatile))\n" \
:::		--vhead "    {\n" \
:::		--vhead "      static const G@Type@Value values[] = {\n" \
:::		--vprod "        { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" },\n" \
:::		--vtail "        { 0, NULL, NULL }\n" \
:::		--vtail "      };\n" \
:::		--vtail "      GType g_define_type_id =\n" \
:::		--vtail "        g_@type@_register_static (g_intern_static_string (\"@EnumName@\"), values);\n" \
:::		--vtail "      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);\n" \
:::		--vtail "    }\n\n" \
:::		--vtail "  return g_define_type_id__volatile;\n" \
:::		--vtail "}\n\n" \
:::		$$(filter-out Makefile, $$^) > $(1).c.tmp && \
:::	(cmp -s $(1).c.tmp $(1).c || cp $(1).c.tmp $(1).c) && \
:::	rm -f $(1).c.tmp && \
:::	echo timestamp > $$@
:::
:::$(1).c: $(1).c.stamp
:::	@true
:::
:::_glib_clean_files += $(1).c $(1).h $(1).c.stamp $(1).h.stamp
:::_glib_built_sources += $(1).c $(1).h
:::_glib-ensure-built-sources: $(1).c $(1).h
endef
_glib_make_mkenums_rules = $(subst :::,,$(_glib_make_mkenums_rules_hidden))

# Run _glib_make_mkenums_rules for each set of generated files
$(foreach f,$(_glib_generated_enum_types),$(eval $(call _glib_make_mkenums_rules,$f)))


### GLIB-GENMARSHAL
# see the comments in the glib-mkenums section for details of how this all works

_glib_marshal_file_prefix = $(subst -,_,$(notdir $(1)))
_glib_marshal_symbol_prefix = $(subst marshal,,$(subst _marshal,,$(subst -,_,$(notdir $(1)))))_marshal
_glib_marshal_sources_var = $(_glib_marshal_file_prefix)_sources
_glib_marshal_sources = $(filter-out $(_glib_built_sources) $(1).c,$($(_glib_marshal_sources_var)))
_glib_marshal_c_sources = $(filter %.c,$(_glib_marshal_sources))

_glib_all_marshal = $(subst .h,,$(notdir $(filter %marshal.h,$(_glib_all_sources))))
_glib_generated_marshal = $(foreach f,$(_glib_all_marshal),$(if $(strip $(call _glib_marshal_sources,$f)),$f))

define _glib_make_genmarshal_rules_hidden
:::$(1).list.stamp: $$(call _glib_marshal_c_sources,$(1)) Makefile
:::	$$(_GLIB_V_GEN) LC_ALL=C sed -ne 's/.*_$(_glib_marshal_symbol_prefix)_\([_A-Z]*\).*/\1/p' $$(filter-out Makefile, $$^) | sort -u | sed -e 's/__/:/' -e 's/_/,/g' > $(1).list.tmp && \
:::	(cmp -s $(1).list.tmp $(1).list || cp $(1).list.tmp $(1).list) && \
:::	rm -f $(1).list.tmp && \
:::	echo timestamp > $$@
:::
:::$(1).list: $(1).list.stamp
:::	@true
:::
:::$(1).h: $(1).list
:::	$$(_GLIB_V_GEN) $$(GLIB_GENMARSHAL) \
:::		--prefix=_$(_glib_marshal_symbol_prefix) --header \
:::		$$(GLIB_GENMARSHAL_H_FLAGS) \
:::		$$($(_glib_marshal_symbol_prefix)_GENMARSHAL_H_FLAGS) \
:::		$$< > $$@.tmp && \
:::	mv $$@.tmp $$@
:::
:::$(1).c: $(1).list
:::	$$(_GLIB_V_GEN) (echo "#include \"$$(subst .c,.h,$$(@F))\""; $$(GLIB_GENMARSHAL) \
:::		--prefix=_$(_glib_marshal_symbol_prefix) --body \
:::		$$(GLIB_GENMARSHAL_C_FLAGS) \
:::		$$($(_glib_marshal_symbol_prefix)_GENMARSHAL_C_FLAGS) \
:::		$$< ) > $$@.tmp && \
:::	mv $$@.tmp $$@
:::
:::_glib_clean_files += $(1).c $(1).h $(1).list $(1).list.stamp
:::_glib_built_sources += $(1).c $(1).h
:::_glib-ensure-built-sources: $(1).c $(1).h
endef
_glib_make_genmarshal_rules = $(subst :::,,$(_glib_make_genmarshal_rules_hidden))

$(foreach f,$(_glib_generated_marshal),$(eval $(call _glib_make_genmarshal_rules,$f)))


# GLIB-COMPILE-SCHEMAS
# see the comments in the glib-mkenums section for details of how this all works

_GLIB_ENUMS_XML_GENERATED = $(filter %.enums.xml,$(gsettingsschema_DATA))
_GLIB_GSETTINGS_SCHEMA_FILES = $(filter %.gschema.xml,$(gsettingsschema_DATA))
_GLIB_GSETTINGS_VALID_FILES = $(subst .xml,.valid,$(_GLIB_GSETTINGS_SCHEMA_FILES))

_glib_enums_xml_prefix = $(subst .,_,$(notdir $(1)))
_glib_enums_xml_sources_var = $(_glib_enums_xml_prefix)_sources
_glib_enums_xml_sources = $(filter-out $(_glib_built_sources),$($(_glib_enums_xml_sources_var)))
_glib_enums_xml_namespace = $(subst .enums.xml,,$(notdir $(1)))

define _glib_make_enums_xml_rule_hidden
:::$(1): $(_glib_enums_xml_sources) Makefile
:::	$$(_GLIB_V_GEN) $$(GLIB_MKENUMS) \
:::		--comments '<!-- @comment@ -->' \
:::		--fhead "<schemalist>" \
:::		--vhead "  <@type@ id='$(_glib_enums_xml_namespace).@EnumName@'>" \
:::		--vprod "    <value nick='@valuenick@' value='@valuenum@'/>" \
:::		--vtail "  </@type@>" \
:::		--ftail "</schemalist>" \
:::		$$(filter-out Makefile, $$^) > $$@.tmp && \
:::	mv $$@.tmp $$@
endef
_glib_make_enums_xml_rule = $(subst :::,,$(_glib_make_enums_xml_rule_hidden))

_GLIB_V_CHECK = $(_glib_v_check_$(V))
_glib_v_check_ = $(_glib_v_check_$(AM_DEFAULT_VERBOSITY))
_glib_v_check_0 = @echo "  CHECK   " $(subst .valid,.xml,$@);

define _glib_make_schema_validate_rule_hidden
:::$(subst .xml,.valid,$(1)): $(_GLIB_ENUMS_XML_GENERATED) $(1)
:::	$$(_GLIB_V_CHECK) $$(GLIB_COMPILE_SCHEMAS) --strict --dry-run $$(addprefix --schema-file=,$$^) && touch $$@
endef
_glib_make_schema_validate_rule = $(subst :::,,$(_glib_make_schema_validate_rule_hidden))

define _glib_make_schema_rules_hidden
:::all-am: $(_GLIB_GSETTINGS_VALID_FILES)
:::
:::install-data-am: glib-install-schemas-hook
:::
:::glib-install-schemas-hook: install-gsettingsschemaDATA
:::	@test -n "$(GSETTINGS_DISABLE_SCHEMAS_COMPILE)$(DESTDIR)" || (echo $(GLIB_COMPILE_SCHEMAS) $(gsettingsschemadir); $(GLIB_COMPILE_SCHEMAS) $(gsettingsschemadir))
:::
:::uninstall-am: glib-uninstall-schemas-hook
:::
:::glib-uninstall-schemas-hook: uninstall-gsettingsschemaDATA
:::	@test -n "$(GSETTINGS_DISABLE_SCHEMAS_COMPILE)$(DESTDIR)" || (echo $(GLIB_COMPILE_SCHEMAS) $(gsettingsschemadir); $(GLIB_COMPILE_SCHEMAS) $(gsettingsschemadir))
:::
:::.PHONY: glib-install-schemas-hook glib-uninstall-schemas-hook
endef
_glib_make_schema_rules = $(subst :::,,$(_glib_make_schema_rules_hidden))

_glib_clean_files += $(_GLIB_ENUMS_XML_GENERATED) $(_GLIB_GSETTINGS_VALID_FILES)

$(foreach f,$(_GLIB_ENUMS_XML_GENERATED),$(eval $(call _glib_make_enums_xml_rule,$f)))
$(foreach f,$(_GLIB_GSETTINGS_SCHEMA_FILES),$(eval $(call _glib_make_schema_validate_rule,$f)))
$(if $(_GLIB_GSETTINGS_SCHEMA_FILES),$(eval $(_glib_make_schema_rules)))


### GLIB-COMPILE-RESOURCES
# see the comments in the glib-mkenums section for details of how this all works

_glib_resources_c_file_prefix = $(subst -,_,$(notdir $(1)))
_glib_resources_c_symbol_prefix = $(subst resources,,$(subst _resources,,$(subst -,_,$(notdir $(1)))))
_glib_resources_c_sources_var = $(_glib_resources_c_file_prefix)_sources
_glib_resources_c_sources = $($(_glib_resources_c_sources_var))
_glib_resources_c_deps = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/$(_glib_resources_c_sources))

_glib_all_ch_resources = $(subst .h,,$(notdir $(filter %resources.h,$(_glib_all_sources))))
_glib_generated_ch_resources = $(foreach f,$(_glib_all_ch_resources),$(if $(strip $(call _glib_resources_c_sources,$f)),$f))

_glib_all_c_resources = $(filter-out $(_glib_all_ch_resources),$(subst .c,,$(notdir $(filter %resources.c,$(_glib_all_sources)))))
_glib_generated_c_resources = $(foreach f,$(_glib_all_c_resources),$(if $(strip $(call _glib_resources_c_sources,$f)),$f))

define _glib_make_h_resources_rules_hidden
:::$(1).h: $(_glib_resources_c_sources) $(_glib_resources_c_deps)
:::	$$(_GLIB_V_GEN) $$(GLIB_COMPILE_RESOURCES) \
:::		--target=$$@ --sourcedir="$(srcdir)" \
:::		--generate-header $(2) \
:::		--c-name $(_glib_resources_c_symbol_prefix) \
:::		$$(GLIB_COMPILE_RESOURCES_FLAGS) \
:::		$$($(_glib_resources_c_file_prefix)_COMPILE_RESOURCES_FLAGS) \
:::		$$<
:::
:::_glib_clean_files += $(1).h
:::_glib_built_sources += $(1).h
:::_glib-ensure-built-sources: $(1).h
endef
_glib_make_h_resources_rules = $(subst :::,,$(_glib_make_h_resources_rules_hidden))

define _glib_make_c_resources_rules_hidden
:::$(1).c: $(_glib_resources_c_sources) $(_glib_resources_c_deps)
:::	$$(_GLIB_V_GEN) $$(GLIB_COMPILE_RESOURCES) \
:::		--target=$$@ --sourcedir="$(srcdir)" \
:::		--generate-source $(2) \
:::		--c-name $(_glib_resources_c_symbol_prefix) \
:::		$$(GLIB_COMPILE_RESOURCES_FLAGS) \
:::		$$($(_glib_resources_c_file_prefix)_COMPILE_RESOURCES_FLAGS) \
:::		$$<
:::
:::_glib_dist_files += $(_glib_resources_c_sources) $(_glib_resources_c_deps)
:::_glib_clean_files += $(1).c
:::_glib_built_sources += $(1).c
:::_glib-ensure-built-sources: $(1).c
endef
_glib_make_c_resources_rules = $(subst :::,,$(_glib_make_c_resources_rules_hidden))

$(foreach f,$(_glib_generated_ch_resources),$(eval $(call _glib_make_h_resources_rules,$f,--manual-register)))
$(foreach f,$(_glib_generated_ch_resources),$(eval $(call _glib_make_c_resources_rules,$f,--manual-register)))
$(foreach f,$(_glib_generated_c_resources),$(eval $(call _glib_make_c_resources_rules,$f,)))


_glib_resources_standalone_prefix = $(subst -,_,$(notdir $(1)))_gresource
_glib_resources_standalone_sources_var = $(_glib_resources_standalone_prefix)_sources
_glib_resources_standalone_sources = $($(_glib_resources_standalone_sources_var))
_glib_resources_standalone_deps = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/$(_glib_resources_standalone_sources))

_glib_all_standalone_resources = $(subst .gresource,,$(notdir $(filter %.gresource,$(_glib_all_data))))
_glib_generated_standalone_resources = $(foreach f,$(_glib_all_standalone_resources),$(if $(strip $(call _glib_resources_standalone_sources,$f)),$f))

define _glib_make_standalone_resources_rules_hidden
:::$(1).gresource: $(_glib_resources_standalone_sources) $(_glib_resources_standalone_deps)
:::	$$(_GLIB_V_GEN) $$(GLIB_COMPILE_RESOURCES) \
:::		--target=$$@ --sourcedir="$(srcdir)" \
:::		$$(GLIB_COMPILE_RESOURCES_FLAGS) \
:::		$$($(_glib_resources_standalone_prefix)_COMPILE_RESOURCES_FLAGS) \
:::		$$<
:::
:::_glib_dist_files += $(_glib_resources_standalone_sources) $(_glib_resources_standalone_deps)
:::_glib_clean_files += $(1).gresource
:::_glib_built_sources += $(1).gresource
:::_glib-ensure-built-sources: $(1).gresource
endef
_glib_make_standalone_resources_rules = $(subst :::,,$(_glib_make_standalone_resources_rules_hidden))

$(foreach f,$(_glib_generated_standalone_resources),$(eval $(call _glib_make_standalone_resources_rules,$f)))
