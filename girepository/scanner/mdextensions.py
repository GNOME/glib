from markdown.extensions import Extension
from markdown.treeprocessors import Treeprocessor


class RemoveOuterP(Treeprocessor):
    def run(self, root):
        if len(root) == 1 and root[0].tag == "p":
            root[0].tag = "span"


class InlineMarkdown(Extension):
    def extendMarkdown(self, md):
        md.treeprocessors.register(RemoveOuterP(md), "remove_outer_p", 0)
