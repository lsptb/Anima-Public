#include <itkImageFileReader.h>
#include <itkImageRegionConstIterator.h>
#include <tclap/CmdLine.h>
#include <fstream>

int main(int argc, char * *argv)
{
    TCLAP::CmdLine cmd("INRIA / IRISA - VisAGeS Team", ' ', ANIMA_VERSION);

    TCLAP::ValueArg <std::string> refArg("r", "reffile", "Reference segmentations", true, "", "reference image", cmd);

    TCLAP::ValueArg <std::string> testArg("t", "testfile", "Test segmentations", true, "", "test image", cmd);

    TCLAP::ValueArg <unsigned int> minNumPixelsRefArg("m", "min-card", "Minimal number of pixels in each reference label area", false, 0, "minimal label size", cmd);
    TCLAP::SwitchArg totalOverlapArg("T", "total-overlap", "Compute total overlap measure as in Klein et al 2009", cmd, false);
    TCLAP::SwitchArg jacArg("J", "jaccard", "Compute Jaccard similarity instead of Dice", cmd, false);
    TCLAP::SwitchArg xlsArg("X", "xlsmode", "Output formatted to be include in a Excel readable text file", cmd, false);

    TCLAP::ValueArg <std::string> fileArg("o", "out-txt", "Output stored into a file", false, "", "Output file", cmd);

    try
    {
        cmd.parse(argc, argv);
    }
    catch (TCLAP::ArgException& e)
    {
        std::cerr << "Error: " << e.error() << "for argument " << e.argId() << std::endl;
        exit(EXIT_FAILURE);
    }

    typedef itk::Image <unsigned short, 3> ImageType;
    typedef itk::ImageFileReader <ImageType> ImageReaderType;
    typedef itk::ImageRegionConstIterator <ImageType> ImageIteratorType;

    ImageReaderType::Pointer refRead = ImageReaderType::New();
    refRead->SetFileName(refArg.getValue());
    refRead->Update();

    ImageIteratorType refIt (refRead->GetOutput(), refRead->GetOutput()->GetLargestPossibleRegion());

    std::vector <unsigned int> usefulLabels;

    while (!refIt.IsAtEnd())
    {
        if (refIt.Get() != 0)
        {
            bool isAlreadyIn = false;
            for (unsigned int i = 0; i < usefulLabels.size(); ++i)
            {
                if (refIt.Get() == usefulLabels[i])
                {
                    isAlreadyIn = true;
                    break;
                }
            }

            if (!isAlreadyIn)
                usefulLabels.push_back(refIt.Get());
        }

        ++refIt;
    }

    std::sort(usefulLabels.begin(), usefulLabels.end());

    std::map <unsigned int, unsigned int> labelBackMap;

    labelBackMap[0] = 0;
    for (unsigned int i = 0; i < usefulLabels.size(); ++i)
    {
        labelBackMap[usefulLabels[i]] = i+1;
    }

    refIt.GoToBegin();

    ImageReaderType::Pointer testRead = ImageReaderType::New();
    testRead->SetFileName(testArg.getValue());
    testRead->Update();

    ImageIteratorType testIt (testRead->GetOutput(), testRead->GetOutput()->GetLargestPossibleRegion());

    unsigned int numLabels = usefulLabels.size();
    std::vector <double> cardRef(numLabels+1, 0), cardTest(numLabels+1, 0), cardInter(numLabels+1, 0);

    while(!testIt.IsAtEnd())
    {
        unsigned int testVal = testIt.Get();
        unsigned int refVal = refIt.Get();

        cardTest[labelBackMap[testVal]]++;
        cardRef[labelBackMap[refVal]]++;

        if (testVal == refVal)
            cardInter[labelBackMap[testVal]]++;

        ++testIt;
        ++refIt;
    }


    //////////////////////////////////////////////////////////////////////////
    // Select and open output
    std::fstream oStremFileOut;
    std::ostream *poOutStreamForResults = &std::cout;
    bool bFileResFaild = false;

    if (fileArg.getValue() != "")
    {
        oStremFileOut.open(fileArg.getValue().c_str(), std::ios::out | std::ios::trunc);
        if(!oStremFileOut.is_open())
        {
            std::cerr << "Can not open file: " << fileArg.getValue() << "to store score" << std::flush;
            bFileResFaild = true;
        }
        else
        {
            poOutStreamForResults = &oStremFileOut;
        }
    }
    std::ostream &roOutStreamForResults = *poOutStreamForResults;

    //////////////////////////////////////////////////////////////////////////
    // Computation
    if (totalOverlapArg.isSet())
    {
        double score = 0;

        unsigned int numUsedLabels = 0;
        for (unsigned int i = 1; i <= numLabels; ++i)
        {
            if (cardRef[i] > minNumPixelsRefArg.getValue())
            {
                score += cardInter[i] / cardRef[i];
                ++numUsedLabels;
            }
        }

        if (numUsedLabels > 0)
            score /= numUsedLabels;


        if (xlsArg.isSet())
            roOutStreamForResults << score << " " << std::flush;
        else
            roOutStreamForResults << "Total overlap score: " << score << std::endl;
    }
    else
    {
        for (unsigned int i = 1; i <= numLabels; ++i)
        {
            if (cardRef[i] <= minNumPixelsRefArg.getValue())
                continue;

            double score = 2*cardInter[i]/(cardRef[i] + cardTest[i]);

            if (jacArg.isSet())
                score = score/(2.0 - score);

            if (xlsArg.isSet())
                roOutStreamForResults << score << " " << std::flush;
            else
                roOutStreamForResults << "Label " << usefulLabels[i-1] << ": " << score << std::endl;
        }
    }


    //////////////////////////////////////////////////////////////////////////
    // Close output
    if (oStremFileOut.is_open())
    {
        oStremFileOut.close();
    }

    if (bFileResFaild)
    {
        exit(EXIT_FAILURE);
    }

    return 0;
}

