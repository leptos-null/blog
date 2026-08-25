# blog

My blog, which is statically hosted on GitHub Pages.

## Layout

The `site` directory contains the static sources served on the blog.

Often, blog posts have supporting materials that don't need to be served; for example, code that analyzes data.
Such supporting materials can be placed within a folder named `_workbench` anywhere within the `site` directory.

Generally, the path of a blog post follows the format `site/posts/{YYYY}/{short-title}`. A post is a directory with an `index.html` file.
Additional assets that are specific to the post (and should be served) should also go in this directory.
