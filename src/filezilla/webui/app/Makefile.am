dist_noinst_DATA = \
	index.html \
	package.json \
	package-lock.json \
	postcss.config.js \
	quasar.config.ts \
	README.md \
	src/shims-vue.d.ts \
	src/composables/catchlie.ts \
	src/api/rename.ts \
	src/api/mkdir.ts \
	src/api/remove.ts \
	src/api/index.ts \
	src/api/auth/refresh.ts \
	src/api/auth/errors.ts \
	src/api/auth/index.ts \
	src/api/auth/isLoggedIn.ts \
	src/api/auth/xhr.ts \
	src/api/auth/login.ts \
	src/api/auth/fetch.ts \
	src/api/auth/logout.ts \
	src/api/auth/needed.ts \
	src/App.vue \
	src/env.d.ts \
	src/components/UnknownDisplay.vue \
	src/components/ErrorBoundary.vue \
	src/components/ShareForm.vue \
	src/components/ImageDisplay.vue \
	src/components/ResourceDisplay.vue \
	src/components/UploadForm.vue \
	src/components/NewFolderForm.vue \
	src/components/DirectoryDisplay.vue \
	src/quasar.d.ts \
	src/css/app.scss \
	src/css/quasar.variables.scss \
	src/layouts/MainLayout.vue \
	src/layouts/LoginLayout.vue \
	src/router/routes.ts \
	src/router/index.ts \
	src/pages/LoginPage.vue \
	src/pages/ErrorNotFound.vue \
	src/pages/HomePage.vue \
	src/pages/SharePage.vue \
	tsconfig.json \
	tsconfig.vue-tsc.json \
	.eslintrc.cjs \
	.eslintignore \
	example.com+2-key.pem \
	example.com+2.pem

EXTRA_DIST = dist/spa

if ENABLE_FZ_WEBUI
install-data-hook:
	@mkdir -p $(datadir)/filezilla-server/
	@rm -rf $(datadir)/filezilla-server/webui
	@cp -a $(srcdir)/dist/spa $(datadir)/filezilla-server/webui

uninstall-hook:
	@rm -rf $(datadir)/filezilla-server/webui
	@rmdir $(datadir)/filezilla-server
endif

