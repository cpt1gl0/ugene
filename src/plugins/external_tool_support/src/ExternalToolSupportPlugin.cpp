/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2018 UniPro <ugene@unipro.ru>
 * http://ugene.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <QCoreApplication>
#include <QDirIterator>
#include <QMenu>

#include <U2Algorithm/CDSearchTaskFactoryRegistry.h>
#include <U2Algorithm/DnaAssemblyAlgRegistry.h>
#include <U2Algorithm/GenomeAssemblyRegistry.h>

#include <U2Core/AppContext.h>
#include <U2Core/DNAAlphabet.h>
#include <U2Core/DNASequenceObject.h>
#include <U2Core/DNASequenceSelection.h>
#include <U2Core/DataBaseRegistry.h>
#include <U2Core/ExternalToolRegistry.h>
#include <U2Core/GAutoDeleteList.h>
#include <U2Core/MultiTask.h>
#include <U2Core/ScriptingToolRegistry.h>
#include <U2Core/U2SafePoints.h>

#include <U2Gui/GUIUtils.h>
#include <U2Gui/ToolsMenu.h>

#include <U2View/ADVConstants.h>
#include <U2View/ADVSequenceObjectContext.h>
#include <U2View/ADVUtils.h>
#include <U2View/AnnotatedDNAView.h>
#include <U2View/DnaAssemblyUtils.h>
#include <U2View/MSAEditor.h>
#include <U2View/MaEditorFactory.h>

#include <U2Test/GTest.h>
#include <U2Test/GTestFrameworkComponents.h>
#include <U2Test/XMLTestFormat.h>

#include "ETSProjectViewItemsContoller.h"
#include "ExternalToolSupportPlugin.h"
#include "ExternalToolSupportSettings.h"
#include "ExternalToolSupportSettingsController.h"

#include "R/RSupport.h"
#include "bedtools/BedToolsWorkersLibrary.h"
#include "bedtools/BedtoolsSupport.h"
#include "bigWigTools/BedGraphToBigWigWorker.h"
#include "bigWigTools/BigWigSupport.h"
#include "blast/BlastAllSupport.h"
#include "blast/BlastAllWorker.h"
#include "blast/FormatDBSupport.h"
#include "blast_plus/AlignToReferenceBlastWorker.h"
#include "blast_plus/BlastDBCmdSupport.h"
#include "blast_plus/BlastPlusSupport.h"
#include "blast_plus/BlastPlusWorker.h"
#include "blast_plus/RPSBlastSupportTask.h"
#include "bowtie/BowtieSettingsWidget.h"
#include "bowtie/BowtieSupport.h"
#include "bowtie/BowtieTask.h"
#include "bowtie/BowtieWorker.h"
#include "bowtie/bowtie_tests/bowtieTests.h"
#include "bowtie2/Bowtie2SettingsWidget.h"
#include "bowtie2/Bowtie2Support.h"
#include "bowtie2/Bowtie2Task.h"
#include "bowtie2/Bowtie2Worker.h"
#include "bwa/BwaMemWorker.h"
#include "bwa/BwaSettingsWidget.h"
#include "bwa/BwaSupport.h"
#include "bwa/BwaTask.h"
#include "bwa/BwaWorker.h"
#include "bwa/bwa_tests/bwaTests.h"
#include "cap3/CAP3Support.h"
#include "cap3/CAP3Worker.h"
#include "ceas/CEASReportWorker.h"
#include "ceas/CEASSupport.h"
#include "clustalo/ClustalOSupport.h"
#include "clustalo/ClustalOWorker.h"
#include "clustalw/ClustalWSupport.h"
#include "clustalw/ClustalWWorker.h"
#include "conduct_go/ConductGOSupport.h"
#include "conduct_go/ConductGOWorker.h"
#include "conservation_plot/ConservationPlotSupport.h"
#include "conservation_plot/ConservationPlotWorker.h"
#include "cufflinks/CuffdiffWorker.h"
#include "cufflinks/CufflinksSupport.h"
#include "cufflinks/CufflinksWorker.h"
#include "cufflinks/CuffmergeWorker.h"
#include "cufflinks/GffreadWorker.h"
#include "cutadapt/CutadaptSupport.h"
#include "cutadapt/CutadaptWorker.h"
#include "fastqc/FastqcSupport.h"
#include "fastqc/FastqcWorker.h"
#include "hmmer/HmmerBuildWorker.h"
#include "hmmer/HmmerSearchTask.h"
#include "hmmer/HmmerSearchWorker.h"
#include "hmmer/HmmerSupport.h"
#include "hmmer/HmmerTests.h"
#include "java/JavaSupport.h"
#include "macs/MACSSupport.h"
#include "macs/MACSWorker.h"
#include "mafft/MAFFTSupport.h"
#include "mafft/MAFFTWorker.h"
#include "mrbayes/MrBayesSupport.h"
#include "mrbayes/MrBayesTests.h"
#include "peak2gene/Peak2GeneSupport.h"
#include "peak2gene/Peak2GeneWorker.h"
#include "perl/PerlSupport.h"
#include "phyml/PhyMLSupport.h"
#include "phyml/PhyMLTests.h"
#include "python/PythonSupport.h"
#include "samtools/BcfToolsSupport.h"
#include "samtools/SamToolsExtToolSupport.h"
#include "samtools/TabixSupport.h"
#include "seqpos/SeqPosSupport.h"
#include "seqpos/SeqPosWorker.h"
#include "snpeff/SnpEffSupport.h"
#include "snpeff/SnpEffWorker.h"
#include "spades/SpadesSettingsWidget.h"
#include "spades/SpadesSupport.h"
#include "spades/SpadesTask.h"
#include "spades/SpadesWorker.h"
#include "spidey/SpideySupport.h"
#include "spidey/SpideySupportTask.h"
#include "stringtie/StringTieSupport.h"
#include "stringtie/StringTieWorker.h"
#include "tcoffee/TCoffeeSupport.h"
#include "tcoffee/TCoffeeWorker.h"
#include "tophat/TopHatSupport.h"
#include "tophat/TopHatWorker.h"
#include "trimmomatic/TrimmomaticSupport.h"
#include "utils/ExternalToolSupportAction.h"
#include "utils/ExternalToolValidateTask.h"
#include "vcftools/VcfConsensusSupport.h"
#include "vcftools/VcfConsensusWorker.h"
#include "vcfutils/VcfutilsSupport.h"

#define EXTERNAL_TOOL_SUPPORT_FACTORY_ID "ExternalToolSupport"
#define TOOLS "tools"

namespace U2 {

extern "C" Q_DECL_EXPORT Plugin * U2_PLUGIN_INIT_FUNC() {
    ExternalToolSupportPlugin * plug = new ExternalToolSupportPlugin();
    return plug;
}

/************************************************************************/
/* SearchToolsInPathTask */
/************************************************************************/
class SearchToolsInPathTask : public Task {
public:
    SearchToolsInPathTask(ExternalToolSupportPlugin *_plugin)
        : Task(ExternalToolSupportPlugin::tr("Search tools in PATH"), TaskFlag_NoRun), plugin(_plugin)
    {

    }

    void prepare() {
        QStringList envList = QProcess::systemEnvironment();
        if(envList.indexOf(QRegExp("PATH=.*",Qt::CaseInsensitive))>=0){
            QString pathEnv = envList.at(envList.indexOf(QRegExp("PATH=.*",Qt::CaseInsensitive)));
            QStringList paths;
#if defined(Q_OS_UNIX)
            paths = pathEnv.split("=").at(1).split(":");
#elif defined(Q_OS_WIN)
            paths = pathEnv.split("=").at(1).split(";");
#endif

            foreach(ExternalTool* curTool, AppContext::getExternalToolRegistry()->getAllEntries()){
                // UGENE-1781: Remove python external tool search in PATH
                // It should be fixed without crutches.
                if (curTool->getName() == ET_PYTHON) {
                    continue;
                }

                if(!curTool->getPath().isEmpty()) {
                    continue;
                }
                foreach(const QString& curPath, paths){
                    QString exePath = curPath+"/"+curTool->getExecutableFileName();
                    QFileInfo fileExe(exePath);
                    if(fileExe.exists() && (curTool->getPath()=="")){
                        ExternalToolJustValidateTask* validateTask=new ExternalToolJustValidateTask(curTool->getName(), exePath);
                        connect(validateTask, SIGNAL(si_stateChanged()), plugin, SLOT(sl_validateTaskStateChanged()));
                        addSubTask(validateTask);
                    }
                }
            }
        }
    }

private:
    ExternalToolSupportPlugin *plugin;
};

/************************************************************************/
/* ExternalToolSupportPlugin */
/************************************************************************/
ExternalToolSupportPlugin::ExternalToolSupportPlugin() :
    Plugin(tr("External tool support"), tr("Runs other external tools")) {
    //External tools registry keeps order of items added
    //it is important because there might be dependencies
    ExternalToolRegistry *etRegistry = AppContext::getExternalToolRegistry();
    CHECK(NULL != etRegistry, );

    // python with modules
    etRegistry->registerEntry(new PythonSupport(ET_PYTHON));
    etRegistry->registerEntry(new PythonModuleDjangoSupport(ET_PYTHON_DJANGO));
    etRegistry->registerEntry(new PythonModuleNumpySupport(ET_PYTHON_NUMPY));

    // Rscript with modules
    etRegistry->registerEntry(new RSupport(ET_R));
    etRegistry->registerEntry(new RModuleGostatsSupport(ET_R_GOSTATS));
    etRegistry->registerEntry(new RModuleGodbSupport(ET_R_GO_DB));
    etRegistry->registerEntry(new RModuleHgu133adbSupport(ET_R_HGU133A_DB));
    etRegistry->registerEntry(new RModuleHgu133bdbSupport(ET_R_HGU133B_DB));
    etRegistry->registerEntry(new RModuleHgu133plus2dbSupport(ET_R_HGU1333PLUS2_DB));
    etRegistry->registerEntry(new RModuleHgu95av2dbSupport(ET_R_HGU95AV2_DB));
    etRegistry->registerEntry(new RModuleMouse430a2dbSupport(ET_R_MOUSE430A2_DB));
    etRegistry->registerEntry(new RModuleCelegansdbSupport(ET_R_CELEGANS_DB));
    etRegistry->registerEntry(new RModuleDrosophila2dbSupport(ET_R_DROSOPHILA2_DB));
    etRegistry->registerEntry(new RModuleOrghsegdbSupport(ET_R_ORG_HS_EG_DB));
    etRegistry->registerEntry(new RModuleOrgmmegdbSupport(ET_R_ORG_MM_EG_DB));
    etRegistry->registerEntry(new RModuleOrgceegdbSupport(ET_R_ORG_CE_EG_DB));
    etRegistry->registerEntry(new RModuleOrgdmegdbSupport(ET_R_ORG_DM_EG_DB));
    etRegistry->registerEntry(new RModuleSeqlogoSupport(ET_R_SEQLOGO));

    //perl
    PerlSupport *perlSupport = new PerlSupport(ET_PERL);
    etRegistry->registerEntry(perlSupport);

    //java
    JavaSupport *javaSupport = new JavaSupport(ET_JAVA);
    etRegistry->registerEntry(javaSupport);

    //Fill ExternalToolRegistry with supported tools

    //ClustalW
    ClustalWSupport* clustalWTool=new ClustalWSupport(ET_CLUSTAL);
    etRegistry->registerEntry(clustalWTool);

    //ClustalO
    ClustalOSupport* clustalOTool=new ClustalOSupport(ET_CLUSTALO);
    etRegistry->registerEntry(clustalOTool);

    //MAFFT
    MAFFTSupport* mAFFTTool=new MAFFTSupport(ET_MAFFT);
    etRegistry->registerEntry(mAFFTTool);

    //T-Coffee
    TCoffeeSupport* tCoffeeTool=new TCoffeeSupport(ET_TCOFFEE);
    etRegistry->registerEntry(tCoffeeTool);

    //MrBayes
    MrBayesSupport* mrBayesTool = new MrBayesSupport(ET_MRBAYES);
    etRegistry->registerEntry(mrBayesTool);

    //PhyML
    PhyMLSupport* phyMlTool = new PhyMLSupport(PhyMLSupport::PhyMlRegistryId);
    etRegistry->registerEntry(phyMlTool);

    if (AppContext::getMainWindow()) {
        clustalWTool->getViewContext()->setParent(this);
        clustalWTool->getViewContext()->init();

        ExternalToolSupportAction* clustalWAction = new ExternalToolSupportAction(tr("Align with ClustalW..."), this, QStringList(ET_CLUSTAL));
        clustalWAction->setObjectName(ToolsMenu::MALIGN_CLUSTALW);
        connect(clustalWAction, SIGNAL(triggered()), clustalWTool, SLOT(sl_runWithExtFileSpecify()));
        ToolsMenu::addAction(ToolsMenu::MALIGN_MENU, clustalWAction);

        clustalOTool->getViewContext()->setParent(this);
        clustalOTool->getViewContext()->init();

        ExternalToolSupportAction* clustalOAction = new ExternalToolSupportAction(tr("Align with ClustalO..."), this, QStringList(ET_CLUSTALO));
        clustalOAction->setObjectName(ToolsMenu::MALIGN_CLUSTALO);
        connect(clustalOAction, SIGNAL(triggered()), clustalOTool, SLOT(sl_runWithExtFileSpecify()));
        ToolsMenu::addAction(ToolsMenu::MALIGN_MENU, clustalOAction);

        mAFFTTool->getViewContext()->setParent(this);
        mAFFTTool->getViewContext()->init();

        ExternalToolSupportAction* mAFFTAction= new ExternalToolSupportAction(tr("Align with MAFFT..."), this, QStringList(ET_MAFFT));
        mAFFTAction->setObjectName(ToolsMenu::MALIGN_MAFFT);
        connect(mAFFTAction, SIGNAL(triggered()), mAFFTTool, SLOT(sl_runWithExtFileSpecify()));
        ToolsMenu::addAction(ToolsMenu::MALIGN_MENU, mAFFTAction);

        tCoffeeTool->getViewContext()->setParent(this);
        tCoffeeTool->getViewContext()->init();

        ExternalToolSupportAction* tCoffeeAction= new ExternalToolSupportAction(tr("Align with T-Coffee..."), this, QStringList(ET_TCOFFEE));
        tCoffeeAction->setObjectName(ToolsMenu::MALIGN_TCOFFEE);
        connect(tCoffeeAction, SIGNAL(triggered()), tCoffeeTool, SLOT(sl_runWithExtFileSpecify()));
        ToolsMenu::addAction(ToolsMenu::MALIGN_MENU, tCoffeeAction);
    }

    //FormatDB
    FormatDBSupport* formatDBTool = new FormatDBSupport(ET_FORMATDB);
    etRegistry->registerEntry(formatDBTool);

    //MakeBLASTDB from BLAST+
    FormatDBSupport* makeBLASTDBTool = new FormatDBSupport(ET_MAKEBLASTDB);
    etRegistry->registerEntry(makeBLASTDBTool);

    //BlastAll
    BlastAllSupport* blastallTool = new BlastAllSupport(ET_BLASTALL);
    etRegistry->registerEntry(blastallTool);

    BlastPlusSupport* blastNPlusTool = new BlastPlusSupport(ET_BLASTN);
    etRegistry->registerEntry(blastNPlusTool);
    BlastPlusSupport* blastPPlusTool = new BlastPlusSupport(ET_BLASTP);
    etRegistry->registerEntry(blastPPlusTool);
    BlastPlusSupport* blastXPlusTool = new BlastPlusSupport(ET_BLASTX);
    etRegistry->registerEntry(blastXPlusTool);
    BlastPlusSupport* tBlastNPlusTool = new BlastPlusSupport(ET_TBLASTN);
    etRegistry->registerEntry(tBlastNPlusTool);
    BlastPlusSupport* tBlastXPlusTool = new BlastPlusSupport(ET_TBLASTX);
    etRegistry->registerEntry(tBlastXPlusTool);
    BlastPlusSupport* rpsblastTool = new BlastPlusSupport(ET_RPSBLAST);
    etRegistry->registerEntry(rpsblastTool);
    BlastDbCmdSupport*  blastDbCmdSupport = new BlastDbCmdSupport();
    etRegistry->registerEntry(blastDbCmdSupport);

    // CAP3
    CAP3Support* cap3Tool = new CAP3Support(ET_CAP3);
    etRegistry->registerEntry(cap3Tool);

    // Bowtie
    BowtieSupport* bowtieSupport = new BowtieSupport(ET_BOWTIE);
    etRegistry->registerEntry(bowtieSupport);
    BowtieSupport* bowtieBuildSupport = new BowtieSupport(ET_BOWTIE_BUILD);
    etRegistry->registerEntry(bowtieBuildSupport);

    // Bowtie 2
    Bowtie2Support* bowtie2AlignSupport = new Bowtie2Support(ET_BOWTIE2_ALIGN);
    Bowtie2Support* bowtie2BuildSupport = new Bowtie2Support(ET_BOWTIE2_BUILD);
    Bowtie2Support* bowtie2InspectSupport = new Bowtie2Support(ET_BOWTIE2_INSPECT);
    etRegistry->registerEntry(bowtie2AlignSupport);
    etRegistry->registerEntry(bowtie2BuildSupport);
    etRegistry->registerEntry(bowtie2InspectSupport);

    // BWA
    BwaSupport* bwaSupport = new BwaSupport(ET_BWA);
    etRegistry->registerEntry(bwaSupport);

    // SPAdes
    SpadesSupport* spadesSupport = new SpadesSupport(ET_SPADES);
    etRegistry->registerEntry(spadesSupport);

    // SAMtools (external tool)
    SamToolsExtToolSupport* samToolsExtToolSupport = new SamToolsExtToolSupport(ET_SAMTOOLS_EXT);
    etRegistry->registerEntry(samToolsExtToolSupport);

    // BCFtools (external tool)
    BcfToolsSupport* bcfToolsSupport = new BcfToolsSupport(ET_BCFTOOLS);
    etRegistry->registerEntry(bcfToolsSupport);

    // Tabix
    TabixSupport* tabixSupport = new TabixSupport(ET_TABIX);
    etRegistry->registerEntry(tabixSupport);

    // VcfConsensus
    VcfConsensusSupport* vcfConsSupport = new VcfConsensusSupport(ET_VCF_CONSENSUS);
    etRegistry->registerEntry(vcfConsSupport);

    // Spidey
    SpideySupport* spideySupport = new SpideySupport(ET_SPIDEY);
    etRegistry->registerEntry(spideySupport);

    //bedtools
    BedtoolsSupport* bedtoolsSupport = new BedtoolsSupport(ET_BEDTOOLS);
    etRegistry->registerEntry(bedtoolsSupport);

    //cutadapt
    CutadaptSupport* cutadaptSupport = new CutadaptSupport(ET_CUTADAPT);
    etRegistry->registerEntry(cutadaptSupport);

    //bigwig
    BigWigSupport* bigwigSupport = new BigWigSupport(ET_BIGWIG);
    etRegistry->registerEntry(bigwigSupport);

    // TopHat
    TopHatSupport* tophatTool = new TopHatSupport(ET_TOPHAT);
    etRegistry->registerEntry(tophatTool);

    // Cufflinks external tools
    CufflinksSupport *cuffcompareTool = new CufflinksSupport(ET_CUFFCOMPARE);
    etRegistry->registerEntry(cuffcompareTool);
    CufflinksSupport *cuffdiffTool = new CufflinksSupport(ET_CUFFDIFF);
    etRegistry->registerEntry(cuffdiffTool);
    CufflinksSupport *cufflinksTool = new CufflinksSupport(ET_CUFFLINKS);
    etRegistry->registerEntry(cufflinksTool);
    CufflinksSupport *cuffmergeTool = new CufflinksSupport(ET_CUFFMERGE);
    etRegistry->registerEntry(cuffmergeTool);
    CufflinksSupport *gffreadTool = new CufflinksSupport(ET_GFFREAD);
    etRegistry->registerEntry(gffreadTool);

    // CEAS
    CEASSupport *ceasTool = new CEASSupport(ET_CEAS);
    etRegistry->registerEntry(ceasTool);

    // MACS
    MACSSupport *macs = new MACSSupport(ET_MACS);
    etRegistry->registerEntry(macs);

    // peak2gene
    Peak2GeneSupport *peak2gene = new Peak2GeneSupport(ET_PEAK2GENE);
    etRegistry->registerEntry(peak2gene);

    //ConservationPlot
    ConservationPlotSupport *conservationPlot = new ConservationPlotSupport(ET_CONSERVATION_PLOT);
    etRegistry->registerEntry(conservationPlot);

    //SeqPos
    SeqPosSupport *seqPos = new SeqPosSupport(ET_SEQPOS);
    etRegistry->registerEntry(seqPos);

    //ConductGO
    ConductGOSupport *conductGO = new ConductGOSupport(ET_GO_ANALYSIS);
    etRegistry->registerEntry(conductGO);

    //Vcfutils
    VcfutilsSupport *vcfutils = new VcfutilsSupport(VcfutilsSupport::TOOL_NAME);
    etRegistry->registerEntry(vcfutils);

    //SnpEff
    SnpEffSupport *snpeff = new SnpEffSupport(ET_SNPEFF);
    etRegistry->registerEntry(snpeff);

    //FastQC
    FastQCSupport *fastqc = new FastQCSupport(ET_FASTQC);
    etRegistry->registerEntry(fastqc);

    // StringTie
    StringTieSupport *stringTie = new StringTieSupport(ET_STRINGTIE);
    etRegistry->registerEntry(stringTie);

    //HMMER
    etRegistry->registerEntry(new HmmerSupport(HmmerSupport::BUILD_TOOL));
    etRegistry->registerEntry(new HmmerSupport(HmmerSupport::SEARCH_TOOL));
    etRegistry->registerEntry(new HmmerSupport(HmmerSupport::PHMMER_TOOL));

    //Trimmomatic
    TrimmomaticSupport *trimmomaticSupport = new TrimmomaticSupport(ET_TRIMMOMATIC);
    etRegistry->registerEntry(trimmomaticSupport);

    if (AppContext::getMainWindow()) {

        etRegistry->setToolkitDescription("BLAST", tr("The <i>Basic Local Alignment Search Tool</i> (BLAST) finds regions of local similarity between sequences. "
                               "The program compares nucleotide or protein sequences to sequence databases and calculates the statistical significance of matches. "
                              "BLAST can be used to infer functional and evolutionary relationships between sequences as well as help identify members of gene families."));

        etRegistry->setToolkitDescription("BLAST+", tr("<i>BLAST+</i> is a new version of the BLAST package from the NCBI."));

        etRegistry->setToolkitDescription("GPU-BLAST+", tr("<i>BLAST+</i> is a new version of the BLAST package from the NCBI."));

        etRegistry->setToolkitDescription("Bowtie", tr("<i>Bowtie<i> is an ultrafast, memory-efficient short read aligner. "
                           "It aligns short DNA sequences (reads) to the human genome at "
                           "a rate of over 25 million 35-bp reads per hour. "
                           "Bowtie indexes the genome with a Burrows-Wheeler index to keep "
                           "its memory footprint small: typically about 2.2 GB for the human "
                           "genome (2.9 GB for paired-end). <a href='http://qt-project.org/doc/qt-4.8/qtextbrowser.html#anchorClicked'>Link text</a> "));

        etRegistry->setToolkitDescription("Cufflinks", tr("<i>Cufflinks</i> assembles transcripts, estimates"
                " their abundances, and tests for differential expression and regulation"
                " in RNA-Seq samples. It accepts aligned RNA-Seq reads and assembles"
                " the alignments into a parsimonious set of transcripts. It also estimates"
                " the relative abundances of these transcripts based on how many reads"
                " support each one, taking into account biases in library preparation protocols. "));

        etRegistry->setToolkitDescription("Bowtie2", tr("<i>Bowtie 2</i> is an ultrafast and memory-efficient tool"
                " for aligning sequencing reads to long reference sequences. It is particularly good"
                " at aligning reads of about 50 up to 100s or 1000s of characters, and particularly"
                " good at aligning to relatively long (e.g. mammalian) genomes."
                " <br/><br/>It indexes the genome with an FM index to keep its memory footprint small:"
                " for the human genome, its memory footprint is typically around 3.2Gb."
                " <br/><br/><i>Bowtie 2</i> supports gapped, local, and paired-end alignment modes."));

        etRegistry->setToolkitDescription("Cistrome", tr("<i>Cistrome</i> is a UGENE version of Cistrome pipeline which also includes some tools useful for ChIP-seq analysis"
                "This pipeline is aimed to provide the following analysis steps: peak calling and annotating, motif search and gene ontology."));

        ExternalToolSupportAction* formatDBAction= new ExternalToolSupportAction(tr("BLAST make database..."), this, QStringList(ET_FORMATDB));
        formatDBAction->setObjectName(ToolsMenu::BLAST_DB);
        connect(formatDBAction, SIGNAL(triggered()), formatDBTool, SLOT(sl_runWithExtFileSpecify()));

        ExternalToolSupportAction* makeBLASTDBAction= new ExternalToolSupportAction(tr("BLAST+ make database..."), this, QStringList(ET_MAKEBLASTDB));
        makeBLASTDBAction->setObjectName(ToolsMenu::BLAST_DBP);
        connect(makeBLASTDBAction, SIGNAL(triggered()), makeBLASTDBTool, SLOT(sl_runWithExtFileSpecify()));

        BlastAllSupportContext *blastAllViewContext = new BlastAllSupportContext(this);
        blastAllViewContext->setParent(this);
        blastAllViewContext->init();

        ExternalToolSupportAction* blastallAction= new ExternalToolSupportAction(tr("BLAST search..."), this, QStringList(ET_BLASTALL));
        blastallAction->setObjectName(ToolsMenu::BLAST_SEARCH);
        connect(blastallAction, SIGNAL(triggered()), blastallTool, SLOT(sl_runWithExtFileSpecify()));

        ExternalToolSupportAction* alignToRefBlastAction = new ExternalToolSupportAction(tr("Map reads to reference..."),
                                                                                         this, QStringList() << ET_FORMATDB << ET_BLASTALL);
        alignToRefBlastAction->setObjectName(ToolsMenu::SANGER_ALIGN);
        connect(alignToRefBlastAction, SIGNAL(triggered(bool)), blastNPlusTool, SLOT(sl_runAlign()));

        BlastPlusSupportContext* blastPlusViewCtx = new BlastPlusSupportContext(this);
        blastPlusViewCtx->setParent(this);//may be problems???
        blastPlusViewCtx->init();
        QStringList toolList;
        toolList << ET_BLASTN << ET_BLASTP << ET_BLASTX << ET_TBLASTN << ET_TBLASTX << ET_RPSBLAST;
        ExternalToolSupportAction* blastPlusAction= new ExternalToolSupportAction(tr("BLAST+ search..."), this, toolList);
        blastPlusAction->setObjectName(ToolsMenu::BLAST_SEARCHP);
        connect(blastPlusAction, SIGNAL(triggered()), blastNPlusTool, SLOT(sl_runWithExtFileSpecify()));

        ExternalToolSupportAction* blastPlusCmdAction= new ExternalToolSupportAction(tr("BLAST+ query database..."), this, QStringList(ET_BLASTDBCMD));
        blastPlusCmdAction->setObjectName(ToolsMenu::BLAST_QUERYP);
        connect(blastPlusCmdAction, SIGNAL(triggered()), blastDbCmdSupport, SLOT(sl_runWithExtFileSpecify()));

        //Add to menu NCBI Toolkit
        ToolsMenu::addAction(ToolsMenu::BLAST_MENU, formatDBAction);
        ToolsMenu::addAction(ToolsMenu::BLAST_MENU, makeBLASTDBAction);
        ToolsMenu::addAction(ToolsMenu::BLAST_MENU, blastallAction);
        ToolsMenu::addAction(ToolsMenu::BLAST_MENU, blastPlusAction);
        ToolsMenu::addAction(ToolsMenu::BLAST_MENU, blastPlusCmdAction);

        ExternalToolSupportAction* cap3Action = new ExternalToolSupportAction(QString(tr("Reads de novo assembly (with %1)...")).arg(cap3Tool->getName()), this, QStringList(cap3Tool->getName()));
        cap3Action->setObjectName(ToolsMenu::SANGER_DENOVO);
        connect(cap3Action, SIGNAL(triggered()), cap3Tool, SLOT(sl_runWithExtFileSpecify()));
        ToolsMenu::addAction(ToolsMenu::SANGER_MENU, cap3Action);
        ToolsMenu::addAction(ToolsMenu::SANGER_MENU, alignToRefBlastAction);

        GObjectViewWindowContext* spideyCtx = spideySupport->getViewContext();
        spideyCtx->setParent(this);
        spideyCtx->init();

        HmmerContext *hmmerContext = new HmmerContext(this);
        hmmerContext->init();
    }

    AppContext::getCDSFactoryRegistry()->registerFactory(new CDSearchLocalTaskFactory(), CDSearchFactoryRegistry::LocalSearch);

    QStringList referenceFormats(BaseDocumentFormats::FASTA);
    QStringList readsFormats;
    readsFormats << BaseDocumentFormats::FASTA;
    readsFormats << BaseDocumentFormats::FASTQ;

    AppContext::getDnaAssemblyAlgRegistry()->registerAlgorithm(new DnaAssemblyAlgorithmEnv(BowtieTask::taskName, new BowtieTaskFactory(),
        new BowtieGUIExtensionsFactory(), true/*Index*/, false /*Dbi*/, true/*Paired-reads*/, referenceFormats, readsFormats));

    AppContext::getDnaAssemblyAlgRegistry()->registerAlgorithm(new DnaAssemblyAlgorithmEnv(BwaTask::ALGORITHM_BWA_ALN, new BwaTaskFactory(),
        new BwaGUIExtensionsFactory(), true/*Index*/, false/*Dbi*/, true/*Paired*/, referenceFormats, readsFormats));

    AppContext::getDnaAssemblyAlgRegistry()->registerAlgorithm(new DnaAssemblyAlgorithmEnv(BwaTask::ALGORITHM_BWA_SW, new BwaTaskFactory(),
        new BwaSwGUIExtensionsFactory(), true/*Index*/, false/*Dbi*/, false/*Paired*/, referenceFormats, readsFormats));

    AppContext::getDnaAssemblyAlgRegistry()->registerAlgorithm(new DnaAssemblyAlgorithmEnv(BwaTask::ALGORITHM_BWA_MEM, new BwaTaskFactory(),
        new BwaMemGUIExtensionsFactory(), true/*Index*/, false/*Dbi*/, true/*Paired*/, referenceFormats, readsFormats));

    readsFormats << BaseDocumentFormats::RAW_DNA_SEQUENCE;
    AppContext::getDnaAssemblyAlgRegistry()->registerAlgorithm(new DnaAssemblyAlgorithmEnv(Bowtie2Task::taskName, new Bowtie2TaskFactory(),
        new Bowtie2GUIExtensionsFactory(), true/*Index*/, false /*Dbi*/, true/*Paired-reads*/, referenceFormats, readsFormats));

    QStringList genomeReadsFormats;
    genomeReadsFormats << BaseDocumentFormats::FASTA;
    genomeReadsFormats << BaseDocumentFormats::FASTQ;

    AppContext::getGenomeAssemblyAlgRegistry()->registerAlgorithm(new GenomeAssemblyAlgorithmEnv(ET_SPADES, new SpadesTaskFactory(),
        new SpadesGUIExtensionsFactory(), genomeReadsFormats));

    {
        GTestFormatRegistry *tfr = AppContext::getTestFramework()->getTestFormatRegistry();
        XMLTestFormat *xmlTestFormat = qobject_cast<XMLTestFormat *>(tfr->findFormat("XML"));
        assert(NULL != xmlTestFormat);

        GAutoDeleteList<XMLTestFactory> *l = new GAutoDeleteList<XMLTestFactory>(this);
        l->qlist = BowtieTests::createTestFactories();

        foreach(XMLTestFactory *f, l->qlist) {
            bool res = xmlTestFormat->registerTestFactory(f);
            Q_UNUSED(res);
            assert(res);
        }
    }
    {
        GTestFormatRegistry *tfr = AppContext::getTestFramework()->getTestFormatRegistry();
        XMLTestFormat *xmlTestFormat = qobject_cast<XMLTestFormat *>(tfr->findFormat("XML"));
        assert(NULL != xmlTestFormat);

        GAutoDeleteList<XMLTestFactory> *l = new GAutoDeleteList<XMLTestFactory>(this);
        l->qlist = BwaTests::createTestFactories();

        foreach(XMLTestFactory *f, l->qlist) {
            bool res = xmlTestFormat->registerTestFactory(f);
            Q_UNUSED(res);
            assert(res);
        }
    }
    {

        GTestFormatRegistry* tfr = AppContext::getTestFramework()->getTestFormatRegistry();
        XMLTestFormat *xmlTestFormat = qobject_cast<XMLTestFormat*>(tfr->findFormat("XML"));
        assert(xmlTestFormat!=NULL);

        GAutoDeleteList<XMLTestFactory>* l = new GAutoDeleteList<XMLTestFactory>(this);
        l->qlist = MrBayesToolTests::createTestFactories();

        foreach(XMLTestFactory* f, l->qlist) {
            bool res = xmlTestFormat->registerTestFactory(f);
            Q_UNUSED(res);
            assert(res);
        }
    }
    {

        GTestFormatRegistry* tfr = AppContext::getTestFramework()->getTestFormatRegistry();
        XMLTestFormat *xmlTestFormat = qobject_cast<XMLTestFormat*>(tfr->findFormat("XML"));
        assert(xmlTestFormat!=NULL);

        GAutoDeleteList<XMLTestFactory>* l = new GAutoDeleteList<XMLTestFactory>(this);
        l->qlist = PhyMLToolTests::createTestFactories();

        foreach(XMLTestFactory* f, l->qlist) {
            bool res = xmlTestFormat->registerTestFactory(f);
            Q_UNUSED(res);
            assert(res);
        }
    }
    {

        GTestFormatRegistry* tfr = AppContext::getTestFramework()->getTestFormatRegistry();
        XMLTestFormat *xmlTestFormat = qobject_cast<XMLTestFormat*>(tfr->findFormat("XML"));
        assert(xmlTestFormat != NULL);

        GAutoDeleteList<XMLTestFactory>* l = new GAutoDeleteList<XMLTestFactory>(this);
        l->qlist = HmmerTests::createTestFactories();

        foreach(XMLTestFactory* f, l->qlist) {
            bool res = xmlTestFormat->registerTestFactory(f);
            Q_UNUSED(res);
            assert(res);
        }
    }

    etRegistry->setManager(&validationManager);
    validationManager.start();

    registerSettingsController();

    registerWorkers();

    if (AppContext::getMainWindow()) {
        //Add project view service
        services.push_back(new ExternalToolSupportService());
    }
}

ExternalToolSupportPlugin::~ExternalToolSupportPlugin(){
    ExternalToolSupportSettings::setExternalTools();
}

void ExternalToolSupportPlugin::registerSettingsController() {
    if (NULL != AppContext::getMainWindow()) {
        AppContext::getAppSettingsGUI()->registerPage(new ExternalToolSupportSettingsPageController());
    }
}

void ExternalToolSupportPlugin::registerWorkers() {
    LocalWorkflow::ClustalWWorkerFactory::init();
    LocalWorkflow::ClustalOWorkerFactory::init();
    LocalWorkflow::MAFFTWorkerFactory::init();

    LocalWorkflow::AlignToReferenceBlastWorkerFactory::init();
    LocalWorkflow::BlastAllWorkerFactory::init();
    LocalWorkflow::BlastPlusWorkerFactory::init();

    LocalWorkflow::TCoffeeWorkerFactory::init();
    LocalWorkflow::CuffdiffWorkerFactory::init();
    LocalWorkflow::CufflinksWorkerFactory::init();
    LocalWorkflow::CuffmergeWorkerFactory::init();
    LocalWorkflow::GffreadWorkerFactory::init();
    LocalWorkflow::TopHatWorkerFactory::init();
    LocalWorkflow::CEASReportWorkerFactory::init();
    LocalWorkflow::MACSWorkerFactory::init();
    LocalWorkflow::Peak2GeneWorkerFactory::init();
    LocalWorkflow::ConservationPlotWorkerFactory::init();
    LocalWorkflow::SeqPosWorkerFactory::init();
    LocalWorkflow::ConductGOWorkerFactory::init();
    LocalWorkflow::CAP3WorkerFactory::init();
    LocalWorkflow::VcfConsensusWorkerFactory::init();
    LocalWorkflow::BwaMemWorkerFactory::init();
    LocalWorkflow::BwaWorkerFactory::init();
    LocalWorkflow::BowtieWorkerFactory::init();
    LocalWorkflow::Bowtie2WorkerFactory::init();
    LocalWorkflow::SlopbedWorkerFactory::init();
    LocalWorkflow::GenomecovWorkerFactory::init();
    LocalWorkflow::BedGraphToBigWigFactory::init();
    LocalWorkflow::SpadesWorkerFactory::init();
    LocalWorkflow::SnpEffFactory::init();
    LocalWorkflow::FastQCFactory::init();
    LocalWorkflow::CutAdaptFastqWorkerFactory::init();
    LocalWorkflow::BedtoolsIntersectWorkerFactory::init();
    LocalWorkflow::HmmerBuildWorkerFactory::init();
    LocalWorkflow::HmmerSearchWorkerFactory::init();
    LocalWorkflow::StringTieWorkerFactory::init();
}

//////////////////////////////////////////////////////////////////////////
// Service
ExternalToolSupportService::ExternalToolSupportService()
: Service(Service_ExternalToolSupport, tr("External tools support"), tr("Provides support to run external tools from UGENE"), QList<ServiceType>() << Service_ProjectView)
{
    projectViewController = NULL;
}

void ExternalToolSupportService::serviceStateChangedCallback(ServiceState oldState, bool enabledStateChanged) {
    Q_UNUSED(oldState);

    if (!enabledStateChanged) {
        return;
    }
    if (isEnabled()) {
        projectViewController = new ETSProjectViewItemsContoller(this);
    } else {
        delete projectViewController; projectViewController = NULL;
    }
}

}
