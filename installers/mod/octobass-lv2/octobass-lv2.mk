######################################
#
# octobass-lv2
#
######################################

OCTOBASS_LV2_VERSION = @OCTOBASS_LV2_GIT_REF@
OCTOBASS_LV2_SITE = https://github.com/gianni-cappelletti/October-Production-Co.git
OCTOBASS_LV2_SITE_METHOD = git
OCTOBASS_LV2_GIT_SUBMODULES = y
OCTOBASS_LV2_BUNDLES = OctoBASS.lv2

OCTOBASS_LV2_CONF_OPTS += -DCMAKE_BUILD_TYPE=Release
OCTOBASS_LV2_CONF_OPTS += -DBUILD_OCTOBIR=OFF
OCTOBASS_LV2_CONF_OPTS += -DBUILD_OCTOBIR_JUCE=OFF
OCTOBASS_LV2_CONF_OPTS += -DBUILD_OCTOBIR_VCV=OFF
OCTOBASS_LV2_CONF_OPTS += -DBUILD_OCTOBASS_JUCE=OFF
OCTOBASS_LV2_CONF_OPTS += -DBUILD_OCTOBASS_LV2=ON
OCTOBASS_LV2_CONF_OPTS += -DOCTOBASS_NAM_A2_FAST=ON

# Shallow fetch of the pinned commit and only the submodules this build uses.
# MOD_PLUGIN_BUILDER_DOWNLOAD_WITH_SUBMODULES clones full history including JUCE,
# a silent multi-minute step that drops the builder.mod.audio connection.
# The archive goes where Buildroot looks before downloading: $(DL_DIR) on 2016.02,
# the per-package $(PKG)_DL_DIR on 2023.11 (darkglass-anagram), which defines _DL_SUBDIR.
OCTOBASS_LV2_SUBMODULES = third_party/WDL third_party/pffft third_party/lv2 third_party/NeuralAmpModelerCore
OCTOBASS_LV2_SOURCE_ARCHIVE = $(if $(OCTOBASS_LV2_DL_SUBDIR),$(OCTOBASS_LV2_DL_DIR),$(DL_DIR))/$(OCTOBASS_LV2_SOURCE)

define OCTOBASS_LV2_DOWNLOAD_USED_SUBMODULES
	if [ ! -e $(OCTOBASS_LV2_SOURCE_ARCHIVE) ]; then                                                \
		rm -rf $(@D) &&                                                                         \
		git init -q $(@D) &&                                                                    \
		git -C $(@D) fetch --depth 1 $(OCTOBASS_LV2_SITE) $(OCTOBASS_LV2_VERSION) &&            \
		git -C $(@D) checkout -q FETCH_HEAD &&                                                  \
		git -C $(@D) submodule update --init --depth 1 -- $(OCTOBASS_LV2_SUBMODULES) &&         \
		git -C $(@D)/third_party/NeuralAmpModelerCore submodule update --init --depth 1 --      \
			Dependencies/eigen &&                                                           \
		mkdir -p $(dir $(OCTOBASS_LV2_SOURCE_ARCHIVE)) &&                                       \
		tar --exclude=".git" -czf $(OCTOBASS_LV2_SOURCE_ARCHIVE) -C $(@D) . &&                  \
		touch $(@D)/.stamp_downloaded $(@D)/.stamp_extracted;                                   \
	fi
endef

OCTOBASS_LV2_PRE_DOWNLOAD_HOOKS += OCTOBASS_LV2_DOWNLOAD_USED_SUBMODULES

define OCTOBASS_LV2_INSTALL_TARGET_CMDS
	install -d $(TARGET_DIR)/usr/lib/lv2/$(OCTOBASS_LV2_BUNDLES)
	cp -rL $(@D)/plugins/octobass/lv2/$(OCTOBASS_LV2_BUNDLES)/* $(TARGET_DIR)/usr/lib/lv2/$(OCTOBASS_LV2_BUNDLES)/
endef

$(eval $(cmake-package))
