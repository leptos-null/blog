# blog

My blog, which is statically hosted on GitHub Pages.

## Layout

The `site` directory contains the static sources served on the blog.

Often, blog posts have supporting materials that don't need to be served; for example, code that analyzes data.
Such supporting materials can be placed within a folder named `_workbench` anywhere within the `site` directory.

Generally, the path of a blog post follows the format `site/posts/{YYYY}/{short-title}`. A post is a directory with an `index.html` file.
Additional assets that are specific to the post (and should be served) should also go in this directory.

## Running locally

Run `python3 serve.py` to simulate the GitHub Pages deployment locally.

Specifically, `serve.py`:
1. Requires request paths to start with `/blog`
    - If the path does not start with `/blog`, the response is a `404`, and the remaining points do not apply
2. Serves the static content in the `site` directory
3. Responds with `404` for files/ directories in a `_workbench` directory
4. Serves [`404.html`](./site/404.html) for `404` responses

## AI Policy

I write all of the content (i.e. "prose") for the blog posts themselves.

In this repo, I may use AI tools (e.g. Claude) to write HTML, CSS, and similar supporting infrastructure.
I may also use AI tools to write `alt` text for images.
